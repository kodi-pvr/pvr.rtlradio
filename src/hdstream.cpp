//---------------------------------------------------------------------------
// Copyright (c) 2020-2022 Michael G. Brehm
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//---------------------------------------------------------------------------

#include "stdafx.h"
#include "hdstream.h"

#include <algorithm>
#include <chrono>
#include <memory.h>

#include "align.h"
#include "string_exception.h"

#pragma warning(push, 4)

// hdstream::MAX_PACKET_QUEUE
//
// Maximum number of queued demux packets
size_t const hdstream::MAX_PACKET_QUEUE = 200;		// ~2sec analog / ~10sec digital

// hdstream::STREAM_ID_AUDIO
//
// Stream identifier for the audio output stream
int const hdstream::STREAM_ID_AUDIO = 1;

//---------------------------------------------------------------------------
// hdstream Constructor (private)
//
// Arguments:
//
//	device			- RTL-SDR device instance
//	tunerprops		- Tuner device properties
//	channelprops	- Channel properties
//	hdprops			- HD Radio digital signal processor properties

hdstream::hdstream(std::unique_ptr<rtldevice> device, struct tunerprops const& tunerprops,
	struct channelprops const& channelprops, struct hdprops const& hdprops) :
	m_device(std::move(device)), m_analogfallback(hdprops.analogfallback), m_muxname(""), 
	m_pcmgain(powf(10.0f, hdprops.outputgain / 10.0f))
{
	// Initialize the RTL-SDR device instance
	m_device->set_frequency_correction(tunerprops.freqcorrection + channelprops.freqcorrection);
	uint32_t samplerate = m_device->set_sample_rate(1488375);
	m_device->set_center_frequency(channelprops.frequency);

	// Adjust the device gain as specified by the channel properties
	m_device->set_automatic_gain_control(channelprops.autogain);
	if(channelprops.autogain == false) m_device->set_gain(channelprops.manualgain);

	// Initialize the HD Radio demodulator
	nrsc5_open_pipe(&m_nrsc5);
	nrsc5_set_mode(m_nrsc5, NRSC5_MODE_FM);
	nrsc5_set_callback(m_nrsc5, nrsc5_callback, this);

	// Initialize the wideband FM demodulator parameters
	tDemodInfo demodinfo = {};
	demodinfo.HiCutmax = 100000;
	demodinfo.HiCut = 100000;
	demodinfo.LowCut = -100000;
	demodinfo.SquelchValue = -160;
	demodinfo.WfmDownsampleQuality = DownsampleQuality::Medium;

	// Initialize the wideband FM demodulator
	m_fmdemod = std::unique_ptr<CDemodulator>(new CDemodulator());
	//m_fmdemod->SetUSFmVersion(false);	
	m_fmdemod->SetInputSampleRate(static_cast<TYPEREAL>(samplerate));
	m_fmdemod->SetDemod(DEMOD_WFM, demodinfo);
	m_fmdemod->SetDemodFreq(0);

	// Initialize the output resampler
	m_fmresampler = std::unique_ptr<CFractResampler>(new CFractResampler());
	m_fmresampler->Init(m_fmdemod->GetInputBufferLimit());

	// Create a worker thread on which to perform demodulation
	scalar_condition<bool> started{ false };
	m_worker = std::thread(&hdstream::transfer, this, std::ref(started));
	started.wait_until_equals(true);
}

//---------------------------------------------------------------------------
// hdstream Destructor

hdstream::~hdstream()
{
	close();
}

//---------------------------------------------------------------------------
// hdstream::canseek
//
// Gets a flag indicating if the stream allows seek operations
//
// Arguments:
//
//	NONE

bool hdstream::canseek(void) const
{
	return false;
}

//---------------------------------------------------------------------------
// hdstream::close
//
// Closes the stream
//
// Arguments:
//
//	NONE

void hdstream::close(void)
{
	m_stop = true;								// Signal worker thread to stop
	if(m_device) m_device->cancel_async();		// Cancel any async read operations
	if(m_worker.joinable()) m_worker.join();	// Wait for thread

	nrsc5_close(m_nrsc5);						// Close NRSC5
	m_nrsc5 = nullptr;							// Reset NRSC5 API handle

	m_device.reset();							// Release RTL-SDR device
}

//---------------------------------------------------------------------------
// hdstream::create (static)
//
// Factory method, creates a new hdstream instance
//
// Arguments:
//
//	device			- RTL-SDR device instance
//	tunerprops		- Tunder device properties
//	channelprops	- Channel properties
//	hdprops			- HD Radio digital signal processor properties

std::unique_ptr<hdstream> hdstream::create(std::unique_ptr<rtldevice> device, struct tunerprops const& tunerprops,
	struct channelprops const& channelprops, struct hdprops const& hdprops)
{
	return std::unique_ptr<hdstream>(new hdstream(std::move(device), tunerprops, channelprops, hdprops));
}

//---------------------------------------------------------------------------
// hdstream::demuxabort
//
// Aborts the demultiplexer
//
// Arguments:
//
//	NONE

void hdstream::demuxabort(void)
{
}

//---------------------------------------------------------------------------
// hdstream::demuxflush
//
// Flushes the demultiplexer
//
// Arguments:
//
//	NONE

void hdstream::demuxflush(void)
{
}

//---------------------------------------------------------------------------
// hdstream::demuxread
//
// Reads the next packet from the demultiplexer
//
// Arguments:
//
//	allocator		- DemuxPacket allocation function

DEMUX_PACKET* hdstream::demuxread(std::function<DEMUX_PACKET*(int)> const& allocator)
{
	// Wait up to 100ms for there to be a packet available for processing, don't use
	// an unconditional wait here; unlike analog radio there may not be data until
	// the digitial signal has been synchronized
	std::unique_lock<std::mutex> lock(m_queuelock);
	if(!m_cv.wait_for(lock, std::chrono::milliseconds(100), [&]() -> bool { return ((m_queue.size() > 0) || m_stopped.load() == true); }))
		return allocator(0);

	// If the worker thread was stopped, check for and re-throw any exception that occurred,
	// otherwise assume it was stopped normally and return an empty demultiplexer packet
	if(m_stopped.load() == true) {

		if(m_worker_exception) std::rethrow_exception(m_worker_exception);
		else return allocator(0);
	}

	// Pop off the topmost object from the queue<> and release the lock
	std::unique_ptr<demux_packet_t> packet(std::move(m_queue.front()));
	m_queue.pop();
	lock.unlock();

	// The packet queue should never have a null packet in it
	assert(packet);
	if(!packet) return allocator(0);

	// Allocate and initialize the DEMUX_PACKET
	DEMUX_PACKET* demuxpacket = allocator(packet->size);
	if(demuxpacket != nullptr) {

		demuxpacket->iStreamId = packet->streamid;
		demuxpacket->iSize = packet->size;
		demuxpacket->duration = packet->duration;
		demuxpacket->dts = packet->dts;
		demuxpacket->pts = packet->pts;
		if(packet->size > 0) memcpy(demuxpacket->pData, packet->data.get(), packet->size);
	}

	return demuxpacket;
}

//---------------------------------------------------------------------------
// hdstream::demuxreset
//
// Resets the demultiplexer
//
// Arguments:
//
//	NONE

void hdstream::demuxreset(void)
{
}

//---------------------------------------------------------------------------
// hdstream::devicename
//
// Gets the device name associated with the stream
//
// Arguments:
//
//	NONE

std::string hdstream::devicename(void) const
{
	return std::string(m_device->get_device_name());
}

//---------------------------------------------------------------------------
// hdstream::enumproperties
//
// Enumerates the stream properties
//
// Arguments:
//
//	callback		- Callback to invoke for each stream

void hdstream::enumproperties(std::function<void(struct streamprops const& props)> const& callback)
{
	// AUDIO STREAM
	//
	streamprops audio = {};
	audio.codec = "pcm_s16le";
	audio.pid = STREAM_ID_AUDIO;
	audio.channels = 2;
	audio.samplerate = 44100;
	audio.bitspersample = 16;
	callback(audio);
}

//---------------------------------------------------------------------------
// hdstream::length
//
// Gets the length of the stream; or -1 if stream is real-time
//
// Arguments:
//
//	NONE

long long hdstream::length(void) const
{
	return -1;
}

//---------------------------------------------------------------------------
// hdstream::muxname
//
// Gets the mux name associated with the stream
//
// Arguments:
//
//	NONE

std::string hdstream::muxname(void) const
{
	return m_muxname;
}

//---------------------------------------------------------------------------
// hdstream::nrsc5_callback (private, static)
//
// NRSC5 library event callback function
//
// Arguments:
//
//	event	- NRSC5 event being raised
//	arg		- Implementation-specific context pointer

void hdstream::nrsc5_callback(nrsc5_event_t const* event, void* arg)
{
	assert(arg != nullptr);
	reinterpret_cast<hdstream*>(arg)->nrsc5_callback(event);
}

//---------------------------------------------------------------------------
// hdstream::nrsc5_callback (private)
//
// NRSC5 library event callback function
//
// Arguments:
//
//	event	- NRSC5 event being raised

void hdstream::nrsc5_callback(nrsc5_event_t const* event)
{
	bool	queued = false;		// Flag if an item was queued

	std::unique_lock<std::mutex> lock(m_queuelock);

	// NRSC5_EVENT_IQ
	//
	// If the HD Radio stream is not generating any audio packets, fall back on the
	// analog signal using the Wideband FM demodulator
	if((event->event == NRSC5_EVENT_IQ) && (m_analogfallback) && (!m_hdaudio)) {

		// The byte count must be exact for the analog demodulator to work right
		assert(event->iq.count == (m_fmdemod->GetInputBufferLimit() * 2));

		// The FM demodulator expects the I/Q samples in the range of -32767.0 through +32767.0
		// (32767.0 / 127.5) = 256.9960784313725
		std::unique_ptr<TYPECPX[]> samples(new TYPECPX[event->iq.count / 2]);
		for(int index = 0; index < m_fmdemod->GetInputBufferLimit(); index++) {

			samples[index] = {

			#ifdef FMDSP_USE_DOUBLE_PRECISION
				(static_cast<TYPEREAL>(reinterpret_cast<uint8_t const*>(event->iq.data)[(index * 2)]) - 127.5) * 256.9960784313725,		// I
				(static_cast<TYPEREAL>(reinterpret_cast<uint8_t const*>(event->iq.data)[(index * 2) + 1]) - 127.5) * 256.9960784313725,	// Q
			#else
				(static_cast<TYPEREAL>(reinterpret_cast<uint8_t const*>(event->iq.data)[(index * 2)]) - 127.5f) * 256.9960784313725f,		// I
				(static_cast<TYPEREAL>(reinterpret_cast<uint8_t const*>(event->iq.data)[(index * 2) + 1]) - 127.5f) * 256.9960784313725f,	// Q
			#endif
			};
		}

		// Process the I/Q data, the original samples buffer can be reused/overwritten as it's processed
		int audiopackets = m_fmdemod->ProcessData(m_fmdemod->GetInputBufferLimit(), samples.get(), samples.get());

		// Remove any RDS data that was generated for now
		tRDS_GROUPS rdsgroup = {};
		while(m_fmdemod->GetNextRdsGroupData(&rdsgroup)) { /* DO NOTHING */ }

		// Resample the audio data into a TYPESTEREO16 buffer. Output rate is always 44.1KHz
		size_t audiosize = audiopackets * sizeof(TYPESTEREO16);
		std::unique_ptr<uint8_t[]> audiodata(new uint8_t[audiosize]);
		audiopackets = m_fmresampler->Resample(audiopackets, (m_fmdemod->GetOutputRate() / 44100),
			samples.get(), reinterpret_cast<TYPESTEREO16*>(audiodata.get()), static_cast<TYPEREAL>(m_pcmgain));

		// Generate and queue the audio packet
		std::unique_ptr<demux_packet_t> packet = std::make_unique<demux_packet_t>();
		packet->streamid = STREAM_ID_AUDIO;
		packet->size = audiopackets * sizeof(TYPESTEREO16);
		packet->duration = (audiopackets / static_cast<double>(44100)) * STREAM_TIME_BASE;
		packet->dts = packet->pts = m_dts;
		packet->data = std::move(audiodata);

		m_dts += packet->duration;

		m_queue.emplace(std::move(packet));
		queued = true;
	}

	// NRSC5_EVENT_AUDIO
	//
	// A digital stream audio packet has been generated
	else if(event->event == NRSC5_EVENT_AUDIO) {

		// When synchronization is first achieved, the samples will have already been
		// processed by the analog Wideband FM implementation above; we want to ignore
		// the first HD audio packet to produce the least audio disruption (imperfect)
		if(m_hdaudio) {

			// Filter out anything other than program zero for now
			if(event->audio.program == 0) {

				// Allocate an initialize a heap buffer and copy the audio data into it
				size_t audiosize = event->audio.count * sizeof(int16_t);
				std::unique_ptr<uint8_t[]> audiodata(new uint8_t[audiosize]);

				// Apply the specified PCM output gain while copying the audio data into the packet buffer
				int16_t* pcmdata = reinterpret_cast<int16_t*>(audiodata.get());
				for(size_t index = 0; index < event->audio.count; index++) 
					pcmdata[index] = static_cast<int16_t>(event->audio.data[index] * m_pcmgain);

				// Generate and queue the audio packet
				std::unique_ptr<demux_packet_t> packet = std::make_unique<demux_packet_t>();
				packet->streamid = STREAM_ID_AUDIO;
				packet->size = static_cast<int>(audiosize);
				packet->duration = (event->audio.count / 2.0 / 44100.0) * STREAM_TIME_BASE;
				packet->dts = packet->pts = m_dts;
				packet->data = std::move(audiodata);

				m_dts += packet->duration;

				m_queue.emplace(std::move(packet));
				queued = true;
			}
		}

		else m_hdaudio = true;				// HD Radio is synced and producing audio
	}

	// NRSC5_EVENT_LOST_SYNC
	//
	// If synchronization has been lost, fall back to using the analog signal
	// until sync has been restored and a digital audio packet can be generated
	else if(event->event == NRSC5_EVENT_LOST_SYNC) m_hdaudio = false;

	// NRSC5_EVENT_BER
	//
	// Reporting the current bit error rate
	else if(event->event == NRSC5_EVENT_BER) m_ber.store(event->ber.cber);

	// NRSC5_EVENT_MER
	//
	// Reporting the current modulatation error ratio
	else if(event->event == NRSC5_EVENT_MER) {

		// Store the higher of the two values instead of the mean, some HD radio stations
		// are allowed to transmit one sideband at a higher power than the other
		m_mer.store(std::max(event->mer.lower, event->mer.upper));
	}

	if(queued) {

		// If the queue size has exceeded the maximum, the packets aren't
		// being processed quickly enough by the demux read function
		if(m_queue.size() > MAX_PACKET_QUEUE) {

			m_queue = demux_queue_t();		// Replace the queue<>

			// Push a DEMUX_SPECIALID_STREAMCHANGE packet into the new queue
			std::unique_ptr<demux_packet_t> packet = std::make_unique<demux_packet_t>();
			packet->streamid = DEMUX_SPECIALID_STREAMCHANGE;
			m_queue.push(std::move(packet));

			// Reset the decode time stamp
			m_dts = STREAM_TIME_BASE;
		}

		m_cv.notify_all();			// Notify queue was updated
	}
}

//---------------------------------------------------------------------------
// hdstream::position
//
// Gets the current position of the stream
//
// Arguments:
//
//	NONE

long long hdstream::position(void) const
{
	return -1;
}

//---------------------------------------------------------------------------
// hdstream::read
//
// Reads data from the live stream
//
// Arguments:
//
//	buffer		- Buffer to receive the live stream data
//	count		- Size of the destination buffer in bytes

size_t hdstream::read(uint8_t* /*buffer*/, size_t /*count*/)
{
	return 0;
}

//---------------------------------------------------------------------------
// hdstream::realtime
//
// Gets a flag indicating if the stream is real-time
//
// Arguments:
//
//	NONE

bool hdstream::realtime(void) const
{
	return true;
}

//---------------------------------------------------------------------------
// hdstream::seek
//
// Sets the stream pointer to a specific position
//
// Arguments:
//
//	position	- Delta within the stream to seek, relative to whence
//	whence		- Starting position from which to apply the delta

long long hdstream::seek(long long /*position*/, int /*whence*/)
{
	return -1;
}

//---------------------------------------------------------------------------
// hdstream::servicename
//
// Gets the service name associated with the stream
//
// Arguments:
//
//	NONE

std::string hdstream::servicename(void) const
{
	return std::string(m_hdaudio ? "Hybrid Digital (HD) Radio" : "Wideband FM radio");
}

//---------------------------------------------------------------------------
// hdstream::signalquality
//
// Gets the signal quality as percentages
//
// Arguments:
//
//	NONE

void hdstream::signalquality(int& quality, int& snr) const
{
	// If digital audio has not been synchronized yet and producing data,
	// fall back on the same logic that Wideband FM Radio uses here
	if((!m_hdaudio) && (m_analogfallback)) {

		TYPEREAL demodquality = 0;
		TYPEREAL demodsnr = 0;

		m_fmdemod->GetSignalLevels(demodquality, demodsnr);

		// For wideband FM, adjust the range such that 80% is nominal for
		// signal quality and 60% is nominal for signal-to-noise; this 
		// adjustment is based on observation and (perceived) output quality
		quality = std::max(0, std::min(100, static_cast<int>(100.0 * (demodquality / 0.80))));
		snr = std::max(0, std::min(100, static_cast<int>(100.0 * (demodsnr / 0.60))));
	}

	else {

		// For signal quality, use the NRSC5 Bit Error Rate (BER). A BER of zero
		// implies ideal signal quality. I have no idea what the BER tolerance
		// is for decoding HD Radio, but from observation a BER with a value
		// higher than 0.1 is effectively undecodable so let's call that zero
		float ber = m_ber.load();
		ber = std::min(std::max(ber, 0.0f), 0.1f) * 100.0f;
		quality = static_cast<int>(((ber - 100.0f) * 100.0f) / -100.0f);

		// For signal-to-noise ratio, use the NRSC5 Modulation Error Ratio (MER).
		// A MER of 14 is apparently the ideal for HD Radio, so for now use
		// a linear scale from (0...13) to define the SNR percentage
		float mer = m_mer.load();
		mer = std::max(std::min(13.0f, mer), 0.0f);
		snr = static_cast<int>((mer * 100.0f) / 13.0f);
	}
}

//---------------------------------------------------------------------------
// hdstream::transfer (private)
//
// Worker thread procedure used to transfer data into the ring buffer
//
// Arguments:
//
//	started		- Condition variable to set when thread has started

void hdstream::transfer(scalar_condition<bool>& started)
{
	assert(m_device);
	assert(m_nrsc5);

	// read_callback_func (local)
	//
	// Asynchronous read callback function for the RTL-SDR device
	auto read_callback_func = [&](uint8_t const* buffer, size_t count) -> void {

		// Pipe the samples into NRSC5, it will invoke the necessary callback(s)
		nrsc5_pipe_samples_cu8(m_nrsc5, const_cast<uint8_t*>(buffer), static_cast<unsigned int>(count));
	};

	// Begin streaming from the device and inform the caller that the thread is running
	m_device->begin_stream();
	started = true;

	// Continuously read data from the device until cancel_async() has been called. Use
	// the analog demodulator's packet size since it has to be precisely what's expected
	try { m_device->read_async(read_callback_func, m_fmdemod->GetInputBufferLimit() * 2); }
	catch(...) { m_worker_exception = std::current_exception(); }

	m_stopped.store(true);					// Thread is stopped
	m_cv.notify_all();						// Unblock any waiters
}

//---------------------------------------------------------------------------

#pragma warning(pop)
