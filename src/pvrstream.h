//-----------------------------------------------------------------------------
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
//-----------------------------------------------------------------------------

#ifndef __PVRSTREAM_H_
#define __PVRSTREAM_H_
#pragma once

#include "props.h"

#include <functional>
#include <kodi/addon-instance/PVR.h>
#include <string>

#pragma warning(push, 4)

//---------------------------------------------------------------------------
// Class pvrstream
//
// Defines the interface required for creating and manipulating PVR streams

class pvrstream
{
public:
  // Constructor / Destructor
  //
  pvrstream() {}
  virtual ~pvrstream() {}

  //-----------------------------------------------------------------------
  // Member Functions

  // canseek
  //
  // Flag indicating if the stream allows seek operations
  virtual bool canseek(void) const = 0;

  // close
  //
  // Closes the stream
  virtual void close(void) = 0;

  // demuxabort
  //
  // Aborts the demultiplexer
  virtual void demuxabort(void) = 0;

  // demuxflush
  //
  // Flushes the demultiplexer
  virtual void demuxflush(void) = 0;

  // demuxread
  //
  // Reads the next packet from the demultiplexer
  virtual DEMUX_PACKET* demuxread(std::function<DEMUX_PACKET*(int)> const& allocator) = 0;

  // demuxreset
  //
  // Resets the demultiplexer
  virtual void demuxreset(void) = 0;

  // devicename
  //
  // Gets the device name associated with the stream
  virtual std::string devicename(void) const = 0;

  // enumproperties
  //
  // Enumerates the stream properties
  virtual void enumproperties(
      std::function<void(struct streamprops const& props)> const& callback) = 0;

  // length
  //
  // Gets the length of the stream
  virtual long long length(void) const = 0;

  // muxname
  //
  // Gets the mux name associated with the stream
  virtual std::string muxname(void) const = 0;

  // position
  //
  // Gets the current position of the stream
  virtual long long position(void) const = 0;

  // read
  //
  // Reads available data from the stream
  virtual size_t read(uint8_t* buffer, size_t count) = 0;

  // realtime
  //
  // Gets a flag indicating if the stream is real-time
  virtual bool realtime(void) const = 0;

  // seek
  //
  // Sets the stream pointer to a specific position
  virtual long long seek(long long position, int whence) = 0;

  // servicename
  //
  // Gets the service name associated with the stream
  virtual std::string servicename(void) const = 0;

  // signalquality
  //
  // Gets the signal quality as percentages
  virtual void signalquality(int& quality, int& snr) const = 0;

private:
  pvrstream(pvrstream const&) = delete;
  pvrstream& operator=(pvrstream const&) = delete;
};

//-----------------------------------------------------------------------------

#pragma warning(pop)

#endif // __PVRSTREAM_H_
