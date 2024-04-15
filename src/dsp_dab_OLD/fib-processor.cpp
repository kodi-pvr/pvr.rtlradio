/*
 *    Copyright (C) 2020
 *    Matthias P. Braendli (matthias.braendli@mpb.li)
 *
 *    Copyright (C) 2014
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Programming
 *
 *    This file is part of the SDR-J (JSDR).
 *    SDR-J is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    SDR-J is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with SDR-J; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  fib and fig processor
 */

#define PRINT_DEBUG 0 // To print debug strings set this to 1

#include "fib-processor.h"

#include "MathHelper.h"
#include "dsp_dab/decoders/data/pad/pad_decoder.h"
#include "utils/charsets.h"
#include "utils/log.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <iso646.h>

/*!
 * User Application Type
 *
 * This defines 11-bit fields to identifies the user application that shall be used to decode the
 * data in the channel identified by SId and SCIdS. The interpretation of this field shall be as
 * defined in ETSI TS 101 756, table 16.
 *
 * Used on @ref CFIBProcessor::HandleFIG0Extension13()
 */
enum class UAtype
{
  //! Reserved for future definition
  Reserved0 = 0x000,

  //! Dynamic labels (X-PAD only)
  //!
  //! Added with ETSI TS 101 756 V1.1.1 (2000-10)
  //! Obsolete since ETSI TS 101 756 V1.2.1 (2005-01)
  DynamicLabels = 0x001,

  //! Hybrid Digital Radio (DAB, DRM, RadioDNS) SlideShow
  //!
  //! See ETSI TS 101 499
  //!
  //! @note Replace @ref MOTSlideshow with same id number.
  //!
  //! Added with ETSI TS 101 756 V1.1.1 (2000-10) as MOTSlideshow
  //! Renamed with ETSI TS 101 756 V2.1.1 (2017-01) to Slideshow
  Slideshow = 0x002,

  //! MOT Broadacst Web Site
  //!
  //! See ETSI TS 101 498.
  //!
  //! Added with ETSI TS 101 756 V1.1.1 (2000-10)
  //! Obsolete since ETSI TS 101 756 V2.1.1 (2017-01)
  MOTBroadacst = 0x003,

  //! Transport of TPEG services
  //!
  //! Srr ETSI TS 103 551.
  //!
  //! Added with ETSI TS 101 756 V1.1.1 (2000-10)
  TPEG = 0x004,

  //! DGPS
  //!
  //! Added with ETSI TS 101 756 V1.1.1 (2000-10)
  //! Obsolete since ETSI TS 101 756 V2.1.1 (2017-01)
  DGPS = 0x005,

  //! DAB-TMC (Traffic Message Channel)"
  //!
  //! See ETSI TS 102 368.
  //!
  //! Added with ETSI TS 101 756 V1.1.1 (2000-10)
  //! Obsolete since ETSI TS 101 756 V2.1.1 (2017-01)
  TMC = 0x006,

  //! Service and Programme Information (SPI) for Hybrid Digital Radio (DAB, DRM, RadioDNS)
  //!
  //! XML Specification for Service and Programme Information (SPI)
  //! see ETSI TS 102 818.
  //!
  //! Added with ETSI TS 101 756 V1.2.1 (2005-01) as EPG
  //! Renamed with ETSI TS 101 756 V2.1.1 (2017-01) to SPI
  SPI = 0x007,

  //! DAB Java
  //!
  //! See ETSI TS 101 993.
  //!
  //! Added with ETSI TS 101 756 V1.2.1 (2005-01)
  //! Obsolete since ETSI TS 101 756 V2.1.1 (2017-01)
  DABJava = 0x008,

  //! DMB video service
  //!
  //! See ETSI TS 102 428.
  //!
  //! Added with ETSI TS 101 756 V1.3.1 (2006-02)
  DMB = 0x009,

  //! IPDC Services; Transport specification
  //!
  //! See ETSI TS 102 978.
  //!
  //! Added with ETSI TS 101 756 V1.4.1 (2009-07)
  //! Obsolete since ETSI TS 101 756 V2.1.1 (2017-01)
  IPDCServices = 0x00a,

  //! Voice Applications
  //!
  //! See ETSI TS 102 632.
  //!
  //! Added with ETSI TS 101 756 V1.4.1 (2009-07)
  //! Obsolete since ETSI TS 101 756 V2.1.1 (2017-01)
  VoiceApplications = 0x00b,

  //! Middleware
  //! Part 1: System aspects
  //!
  //! See ETSI TS 102 635.
  //!
  //! Added with ETSI TS 101 756 V1.4.1 (2009-07)
  //! Obsolete since ETSI TS 101 756 V2.1.1 (2017-01)
  Middleware = 0x00c,

  //! Filecasting
  //!
  //! See ETSI TS 103 177.
  //!
  //! Added with ETSI TS 101 756 V1.5.1 (2013-08)
  Filecasting = 0x00d,

  //! Filtered Information Service (FIS)
  //!
  //! See ETSI TS 103 689.
  //!
  //! Added with ETSI TS 101 756 V2.3.1 (2019-11)
  FIS = 0x00e,

  //! Reserved for future definition, first area (first id)
  Reserved1Begin = 0x00f,

  //! Reserved for future definition, first area (last id)
  Reserved1End = 0x449,

  //! Journaline®
  //!
  //! See Fraunhofer IIS and ETSI TS 102 979.
  //!
  //! Added with ETSI TS 101 756 V1.2.1 (2005-01)
  Journaline = 0x44a,

  //! Reserved for proprietary applications, second area (first id)
  Reserved2Begin = 0x44b,

  //! Reserved for proprietary applications, second area (last id)
  Reserved2End = 0x7ff,
};

/*!
 * Data Service Component Type (DSCTy) 
 * 
 * See clause 6.3.1 of ETSI EN 300 401.
 * 
 * Contains all presently registered Data Service Component Types and their bit
 * values. All the remaining values (not shown) are reserved for future use.
 * 
 * See ETSI TS 101 756 V2.4.1 (2020-08) clause 5.3.
 */
enum class DSCType
{

};

FIBProcessor::FIBProcessor(RadioControllerInterface& mr) : myRadioInterface(mr)
{
  clearEnsemble();
}

//  FIB's are segments of 256 bits. When here, we already
//  passed the crc and we start unpacking into FIGs
//  This is merely a dispatcher
void FIBProcessor::processFIB(uint8_t* p, uint16_t fib)
{
  int8_t processedBytes = 0;
  uint8_t* d = p;

  std::lock_guard<std::mutex> lock(mutex);

  (void)fib;
  while (processedBytes < 30) 
  {
    const uint8_t FIGtype = getBits_3(d, 0);
    switch (FIGtype) 
    {
    case 0:
      process_FIG0(d);
      break;

    case 1:
      process_FIG1(d);
      break;

    case 2:
      process_FIG2(d);
      break;

    case 6:
      process_FIG6(d);
      return;

    case 7:
      process_FIG7(d);
      return;

    default:
      process_FIGUnsupported(d);
      //std::clog << "FIG%d present" << FIGtype << std::endl;
      break;
    }

    //  Thanks to Ronny Kunze, who discovered that I used
    //  a p rather than a d
    processedBytes += getBits_5(d, 3) + 1;
    d = p + processedBytes * 8;
  }
}

//
//  Handle ensemble is all through FIG0
//
void FIBProcessor::process_FIG0(uint8_t* d)
{
  const uint8_t extension = getBits_5(d, 8 + 3);
  //uint8_t   CN  = getBits_1 (d, 8 + 0);

  switch (extension)
  {
    case 0:
      FIG0Extension0(d);
      break;
    case 1:
      FIG0Extension1(d);
      break;
    case 2:
      FIG0Extension2(d);
      break;
    case 3:
      FIG0Extension3(d);
      break;
    case 4:
      FIG0ExtensionUnsupported(d);
      break;
    case 5:
      FIG0Extension5(d);
      break;
    case 6:
      FIG0ExtensionUnsupported(d);
      break;
    case 7:
      FIG0Extension7(d);
      break;
    case 8:
      FIG0Extension8(d);
      break;
    case 9:
      FIG0Extension9(d);
      break;
    case 10:
      FIG0Extension10(d);
      break;
    case 11:
      FIG0ExtensionUnsupported(d);
      break;
    case 12:
      FIG0ExtensionUnsupported(d);
      break;
    case 13:
      FIG0Extension13(d);
      break;
    case 14:
      FIG0Extension14(d);
      break;
    case 15:
      FIG0ExtensionUnsupported(d);
      break;
    case 16:
      FIG0ExtensionUnsupported(d);
      break;
    case 17:
      FIG0Extension17(d);
      break;
    case 18:
      FIG0Extension18(d);
      break;
    case 19:
      FIG0Extension19(d);
      break;
    case 20:
      FIG0ExtensionUnsupported(d);
      break;
    case 21:
      FIG0Extension21(d);
      break;
    case 22:
      FIG0Extension22(d);
      break;
    case 23:
      FIG0ExtensionUnsupported(d);
      break;
    case 24:
      FIG0ExtensionUnsupported(d);
      break;
    case 25:
      FIG0ExtensionUnsupported(d);
      break;
    case 26:
      FIG0ExtensionUnsupported(d);
      break;
    case 27:
      FIG0ExtensionUnsupported(d);
      break;
    case 28:
      FIG0ExtensionUnsupported(d);
      break;
    case 29:
      FIG0ExtensionUnsupported(d);
      break;
    case 30:
      FIG0ExtensionUnsupported(d);
      break;
    case 31:
      FIG0ExtensionUnsupported(d);
      break;
    default:
      break;
  }
}

//  FIG0/0 indicated a change in channel organization
//  we are not equipped for that, so we just return
//  control to the init
void FIBProcessor::FIG0Extension0(uint8_t* d)
{
  uint8_t changeflag;
  uint16_t highpart, lowpart;
  int16_t occurrenceChange;
  uint8_t CN = getBits_1(d, 8 + 0);
  //uint8_t OE = getBits_1(d, 8 + 1);
  //uint8_t P_D = getBits_1(d, 8 + 2);
  (void)CN;

  uint16_t eId = getBits(d, 16, 16);

  if (ensembleId != eId)
  {
    ensembleId = eId;
    myRadioInterface.onNewEnsemble(ensembleId);
  }

  changeflag = getBits_2(d, 16 + 16);

  highpart = getBits_5(d, 16 + 19) % 20;
  (void)highpart;
  lowpart = getBits_8(d, 16 + 24) % 250;
  (void)lowpart;
  occurrenceChange = getBits_8(d, 16 + 32);
  (void)occurrenceChange;

  // In transmission mode I, because four ETI frames make one transmission frame, we will
  // see lowpart == 0 only every twelve seconds, and not 6 as expected by the 250 overflow value.
  if (lowpart == 0)
  {
    timeLastFCT0Frame = std::chrono::system_clock::now();
  }

  if (changeflag == 0)
    return;
  else if (changeflag == 1)
  {
    DBGLOG("fib-processor: Changes in sub channel organization\n");
    DBGLOG("fib-processor: cifcount = %d\n", highpart * 250 + lowpart);
    DBGLOG("fib-processor: Change happening in %d CIFs\n", occurrenceChange);
  }
  else if (changeflag == 3)
  {
    DBGLOG("fib-processor: Changes in subchannel and service organization\n");
    DBGLOG("fib-processor: cifcount = %d\n", highpart * 250 + lowpart);
    DBGLOG("fib-processor: Change happening in %d CIFs\n", occurrenceChange);
  }
  DBGLOG("fib-processor:  changes in config not supported, choose again");
}

//  FIG0 extension 1 creates a mapping between the
//  sub channel identifications and the positions in the
//  relevant CIF.
void FIBProcessor::FIG0Extension1(uint8_t* d)
{
  int16_t used = 2; // offset in bytes
  int16_t Length = getBits_5(d, 3);
  uint8_t PD_bit = getBits_1(d, 8 + 2);
  //uint8_t   CN  = getBits_1 (d, 8 + 0);

  while (used < Length - 1)
    used = HandleFIG0Extension1(d, used, PD_bit);
}

//  defining the channels
int16_t FIBProcessor::HandleFIG0Extension1(uint8_t* d, int16_t offset, uint8_t pd)
{
  int16_t bitOffset = offset * 8;
  const int16_t subChId = getBits_6(d, bitOffset);
  const int16_t startAdr = getBits(d, bitOffset + 6, 10);
  subChannels[subChId].programmeNotData = pd;
  subChannels[subChId].subChId = subChId;
  subChannels[subChId].startAddr = startAdr;
  if (getBits_1(d, bitOffset + 16) == 0)
  { // UEP, short form
    int16_t tableIx = getBits_6(d, bitOffset + 18);
    auto& ps = subChannels[subChId].protectionSettings;
    ps.uepTableIndex = tableIx;
    ps.shortForm = true;
    ps.uepLevel = ProtLevel[tableIx][1];

    subChannels[subChId].length = ProtLevel[tableIx][0];
    bitOffset += 24;
  }
  else
  { // EEP, long form
    auto& ps = subChannels[subChId].protectionSettings;
    ps.shortForm = false;
    int16_t option = getBits_3(d, bitOffset + 17);
    if (option == 0)
    {
      ps.eepProfile = EEPProtectionProfile::EEP_A;
    }
    else if (option == 1)
    {
      ps.eepProfile = EEPProtectionProfile::EEP_B;
    }

    if (option == 0 or // EEP-A protection
        option == 1)
    { // EEP-B protection
      int16_t protLevel = getBits_2(d, bitOffset + 20);
      switch (protLevel)
      {
        case 0:
          ps.eepLevel = EEPProtectionLevel::EEP_1;
          break;
        case 1:
          ps.eepLevel = EEPProtectionLevel::EEP_2;
          break;
        case 2:
          ps.eepLevel = EEPProtectionLevel::EEP_3;
          break;
        case 3:
          ps.eepLevel = EEPProtectionLevel::EEP_4;
          break;
        default:
          DBGLOG("Warning, FIG0/1 for %i has invalid EEP protection level %i\n", subChId,
                 protLevel);
          break;
      }

      int16_t subChanSize = getBits(d, bitOffset + 22, 10);
      subChannels[subChId].length = subChanSize;
    }
    else
    {
      DBGLOG("Warning, FIG0/1 for %i has invalid protection option %i", subChId, option);
    }

    bitOffset += 32;
  }

  return bitOffset / 8; // we return bytes
}

void FIBProcessor::FIG0Extension2(uint8_t* d)
{
  int16_t used = 2; // offset in bytes
  int16_t Length = getBits_5(d, 3);
  uint8_t PD_bit = getBits_1(d, 8 + 2);
  uint8_t CN = getBits_1(d, 8 + 0);

  while (used < Length)
  {
    used = HandleFIG0Extension2(d, used, CN, PD_bit);
  }
}

//  Note Offset is in bytes
//  With FIG0/2 we bind the channels to Service Ids
int16_t FIBProcessor::HandleFIG0Extension2(uint8_t* d, int16_t offset, uint8_t cn, uint8_t pd)
{
  (void)cn;
  int16_t lOffset = 8 * offset;
  int16_t i;
  uint8_t ecc;
  uint8_t cId;
  uint32_t SId;
  int16_t numberofComponents;

  if (pd == 1)
  { // long Sid
    ecc = getBits_8(d, lOffset);
    (void)ecc;
    cId = getBits_4(d, lOffset + 1);
    SId = getBits(d, lOffset, 32);
    lOffset += 32;
  }
  else
  {
    cId = getBits_4(d, lOffset);
    (void)cId;
    SId = getBits(d, lOffset + 4, 12);
    SId = getBits(d, lOffset, 16);
    lOffset += 16;
  }

  // Keep track how often we see a service using a saturating counter.
  // Every time a service is signalled, we increment the counter.
  // If the counter is >= 2, we consider the service. Every second, we
  // decrement all counters by one.
  // This avoids that misdecoded services appear and stay in the list.
  using namespace std::chrono;
  const auto now = steady_clock::now();
  if (timeLastServiceDecrement + seconds(1) < now)
  {

    auto it = serviceRepeatCount.begin();
    while (it != serviceRepeatCount.end())
    {
      if (it->second > 0)
      {
        it->second--;
        ++it;
      }
      else if (it->second == 0)
      {
        dropService(it->second);
        it = serviceRepeatCount.erase(it);
      }
      else
      {
        ++it;
      }
    }

    timeLastServiceDecrement = now;

#if 0
        std::stringstream ss;
        ss << "Counters: ";
        for (auto& c : serviceRepeatCount) {
            ss << " " << c.first << ":" << (int)c.second;
        }
        std::cerr << ss.str() << std::endl;
#endif
  }

  if (serviceRepeatCount[SId] < 4)
  {
    serviceRepeatCount[SId]++;
  }

  if (findServiceId(SId) == nullptr and serviceRepeatCount[SId] >= 2)
  {
    services.emplace_back(SId);
    myRadioInterface.onServiceDetected(SId);
  }

  numberofComponents = getBits_4(d, lOffset + 4);
  lOffset += 8;

  for (i = 0; i < numberofComponents; i++)
  {
    uint8_t TMid = getBits_2(d, lOffset);
    if (TMid == 00)
    { // Audio
      uint8_t ASCTy = getBits_6(d, lOffset + 2);
      uint8_t SubChId = getBits_6(d, lOffset + 8);
      uint8_t PS_flag = getBits_1(d, lOffset + 14);
      bindAudioService(TMid, SId, i, SubChId, PS_flag, ASCTy);
    }
    else if (TMid == 1)
    { // MSC stream data
      uint8_t DSCTy = getBits_6(d, lOffset + 2);
      uint8_t SubChId = getBits_6(d, lOffset + 8);
      uint8_t PS_flag = getBits_1(d, lOffset + 14);
      bindDataStreamService(TMid, SId, i, SubChId, PS_flag, DSCTy);
    }
    else if (TMid == 3)
    { // MSC packet data
      int16_t SCId = getBits(d, lOffset + 2, 12);
      uint8_t PS_flag = getBits_1(d, lOffset + 14);
      uint8_t CA_flag = getBits_1(d, lOffset + 15);
      bindPacketService(TMid, SId, i, SCId, PS_flag, CA_flag);
    }
    else
    {
      // reserved
    }
    lOffset += 16;
  }
  return lOffset / 8; // in Bytes
}

//      The Extension 3 of FIG type 0 (FIG 0/3) gives
//      additional information about the service component
//      description in packet mode.
//      manual: page 55
void FIBProcessor::FIG0Extension3(uint8_t* d)
{
  int16_t used = 2;
  int16_t Length = getBits_5(d, 3);

  while (used < Length)
  {
    used = HandleFIG0Extension3(d, used);
  }
}

//      DSCTy   DataService Component Type
int16_t FIBProcessor::HandleFIG0Extension3(uint8_t* d, int16_t used)
{
  int16_t SCId = getBits(d, used * 8, 12);
  //int16_t CAOrgflag       = getBits_1 (d, used * 8 + 15);
  int16_t DGflag = getBits_1(d, used * 8 + 16);
  int16_t DSCTy = getBits_6(d, used * 8 + 18);
  int16_t SubChId = getBits_6(d, used * 8 + 24);
  int16_t packetAddress = getBits(d, used * 8 + 30, 10);
  //uint16_t        CAOrg   = getBits (d, used * 8 + 40, 16);

  ServiceComponent* packetComp = findPacketComponent(SCId);

  used += 56 / 8;
  if (packetComp)
  {
    packetComp->subchannelId = SubChId;
    packetComp->DSCTy = DSCTy;
    packetComp->DGflag = DGflag;
    packetComp->packetAddress = packetAddress;
  }
  return used;
}

void FIBProcessor::FIG0Extension5(uint8_t* d)
{
  int16_t used = 2; // offset in bytes
  int16_t Length = getBits_5(d, 3);

  while (used < Length)
  {
    used = HandleFIG0Extension5(d, used);
  }
}

int16_t FIBProcessor::HandleFIG0Extension5(uint8_t* d, int16_t offset)
{
  int16_t loffset = offset * 8;
  uint8_t lsFlag = getBits_1(d, loffset);
  int16_t subChId, serviceComp, language;

  if (lsFlag == 0)
  { // short form
    if (getBits_1(d, loffset + 1) == 0)
    {
      subChId = getBits_6(d, loffset + 2);
      language = getBits_8(d, loffset + 8);
      subChannels[subChId].language = language;
    }
    loffset += 16;
  }
  else
  { // long form
    serviceComp = getBits(d, loffset + 4, 12);
    language = getBits_8(d, loffset + 16);
    loffset += 24;
  }
  (void)serviceComp;

  return loffset / 8;
}

/*!
 * @brief Configuration information
 *
 * See ETSI EN 300 401 V2.1.1 (2017-01) clause 6.4.2.
 */
void FIBProcessor::FIG0Extension7(uint8_t* d)
{
  const uint8_t servicesAmount = getBits_6(d, 16);
  const uint16_t reconfCount = getBits(d, 16 + 6, 10);

  DBGLOG("void FIBProcessor::FIG0Extension7: servicesAmount=%i, reconfCount=%i\n", servicesAmount,
         reconfCount);
}

void FIBProcessor::FIG0Extension8(uint8_t* d)
{
  int16_t used = 2; // offset in bytes
  int16_t Length = getBits_5(d, 3);
  uint8_t PD_bit = getBits_1(d, 8 + 2);

  while (used < Length)
  {
    used = HandleFIG0Extension8(d, used, PD_bit);
  }
}

int16_t FIBProcessor::HandleFIG0Extension8(uint8_t* d, int16_t used, uint8_t pdBit)
{
  int16_t lOffset = used * 8;
  uint32_t SId = getBits(d, lOffset, pdBit == 1 ? 32 : 16);
  lOffset += (pdBit == 1 ? 32 : 16);

  uint8_t extensionFlag;

  extensionFlag = getBits_1(d, lOffset);
  uint16_t SCIds = getBits_4(d, lOffset + 4);
  lOffset += 4;

  uint8_t lsFlag = getBits_1(d, lOffset);
  if (lsFlag == 1)
  {
    int16_t SCid = getBits(d, lOffset + 4, 12);
    lOffset += 16;
    if (findPacketComponent((SCIds << 4) | SCid) != NULL)
    {
      DBGLOG("fib-processor: packet component bestaat !!\n");
    }
  }
  else
  {
    //int16_t SubChId = getBits_6(d, lOffset + 4);
    lOffset += 8;
  }

  if (extensionFlag)
  {
    lOffset += 8; // skip Rfa
  }
  (void)SId;
  (void)SCIds;
  return lOffset / 8;
}

//  FIG0/9 and FIG0/10 are copied from the work of
//  Michael Hoehn
void FIBProcessor::FIG0Extension9(uint8_t* d)
{
  int16_t offset = 16;

  dateTime.hourOffset =
      (getBits_1(d, offset + 2) == 1) ? -1 * getBits_4(d, offset + 3) : getBits_4(d, offset + 3);
  dateTime.minuteOffset = (getBits_1(d, offset + 7) == 1) ? 30 : 0;
  timeOffsetReceived = true;

  ensembleEcc = getBits(d, offset + 8, 8);
}

void FIBProcessor::FIG0Extension10(uint8_t* fig)
{
  int16_t offset = 16;
  int32_t mjd = getBits(fig, offset + 1, 17);
  // Convert Modified Julian Date (according to wikipedia)
  int32_t J = mjd + 2400001;
  int32_t j = J + 32044;
  int32_t g = j / 146097;
  int32_t dg = j % 146097;
  int32_t c = ((dg / 36524) + 1) * 3 / 4;
  int32_t dc = dg - c * 36524;
  int32_t b = dc / 1461;
  int32_t db = dc % 1461;
  int32_t a = ((db / 365) + 1) * 3 / 4;
  int32_t da = db - a * 365;
  int32_t y = g * 400 + c * 100 + b * 4 + a;
  int32_t m = ((da * 5 + 308) / 153) - 2;
  int32_t d = da - ((m + 4) * 153 / 5) + 122;
  int32_t Y = y - 4800 + ((m + 2) / 12);
  int32_t M = ((m + 2) % 12) + 1;
  int32_t D = d + 1;

  dateTime.year = Y;
  dateTime.month = M;
  dateTime.day = D;
  dateTime.hour = getBits_5(fig, offset + 21);
  if (getBits_6(fig, offset + 26) != dateTime.minutes)
    dateTime.seconds = 0; // handle overflow

  dateTime.minutes = getBits_6(fig, offset + 26);
  if (fig[offset + 20] == 1)
  {
    dateTime.seconds = getBits_6(fig, offset + 32);
  }

  if (timeOffsetReceived)
  {
    myRadioInterface.onDateTimeUpdate(dateTime);
  }
}

#undef DBGLOG
#define DBGLOG(f, ...) { utils::DEBUG_PRINT(f, ##__VA_ARGS__); }

void FIBProcessor::FIG0Extension13(uint8_t* d)
{
  int16_t used = 2; // offset in bytes
  uint8_t Length = getBits_5(d, 3);
  uint8_t CN_bit = getBits_1(d, 8 + 0);
  uint8_t OE_bit = getBits_1(d, 8 + 1);
  uint8_t PD_bit = getBits_1(d, 8 + 2);

  DBGLOG("FIBProcessor::FIG0Extension13: Length: %i CN_bit: %i OE_bit: %i PD_bit: %i\n", Length, CN_bit, OE_bit, PD_bit);

  while (used < Length)
  {
    used = HandleFIG0Extension13(d, used, CN_bit, OE_bit, PD_bit);
  }
}

/*
 * @brief User application information
 * 
 * See ETSI EN 300 401 V2.1.1 (2017-01) clause 6.3.6.
 */
int16_t FIBProcessor::HandleFIG0Extension13(uint8_t* d, int16_t used, uint8_t CN_bit, uint8_t OE_bit, uint8_t PD_bit)
{
  int16_t bitOffset = used * 8;

  const uint8_t SIdLength = PD_bit == 1 ? 32 : 16; // see clause 5.2.2.1 about bit select
  const uint32_t SId = getBits(d, bitOffset, SIdLength); // Get Service Identifier
  const uint8_t SCIdS = getBits_4(d, bitOffset += SIdLength); // Service Component Identifier
  const uint8_t amountOfApps = getBits_4(d, bitOffset + 4); // Number of user applications

  bitOffset += 8;

  DBGLOG("FIBProcessor::HandleFIG0Extension13: Number of user apps: %i\n", amountOfApps);

  for (uint8_t i = 0; i < amountOfApps; i++)
  {
    const UAtype appType = static_cast<UAtype>(getBits(d, bitOffset, 11));
    const uint16_t length = getBits_5(d, bitOffset += 11);
    bitOffset += 5;

    uint16_t bitOffsetNext = bitOffset + 8 * length;

    switch (appType)
    {
      //! Reserved for future use
      case UAtype::Reserved0:
      {
        break;
      }

      //! Dynamic Labels
      //! Obsolete since ETSI TS 101 756 V1.2.1 (2005-01)
      case UAtype::DynamicLabels:
      {
        DBGLOG(" - No. %02i: Dynamic Labels\n", i);
        break;
      }

      //! Slideshow
      case UAtype::Slideshow:
      {
        const bool CAFlag = getBits_1(d, bitOffset) != 0;
        const bool CAOrgFlag = getBits_1(d, bitOffset += 1) != 0;
        bitOffset += 1; // Rfu1 ignored
        const X_PADApplicationType X_PADAppType =
            static_cast<X_PADApplicationType>(getBits_5(d, bitOffset += 1));
        const bool DGflag = getBits_1(d, bitOffset += 5) != 0;
        bitOffset += 1; // Rfu2 ignored
        const uint8_t DSCTy = getBits_6(d, bitOffset += 1);
        const uint16_t CAOrg = getBits(d, bitOffset += 6, 16);
        bitOffset += 16;

        uint8_t* data = static_cast<uint8_t*>(malloc(length));
        for (int i = 0; i < length - 4; i++)
        {
          data[i] = getBits_8(d, bitOffset + i * 8);
          fprintf(stderr, "%X", data[i]);
        }
        fprintf(stderr, "\n");
        m_MOTManager.HandleMOTDataGroup(data, length-4);
        free(data);

        DBGLOG(" - No. %02i: MOT slideshow: CAFlag='%s', CAOrgFlag='%s', X_PADAppType='%u', "
          "DGflag='%s', DSCTy='%u', CAOrg='0x02%X', length='%u'\n",
          i, CAFlag ? "true" : "false", CAOrgFlag ? "true" : "false", X_PADAppType,
          DGflag ? "true" : "false", DSCTy, CAOrg, length);
        break;
#undef DBGLOG
#define DBGLOG
      }

      //! Broadcast Web Site
      //! Obsolete since ETSI TS 101 756 V2.1.1 (2017-01)
      case UAtype::MOTBroadacst:
      {
        DBGLOG(" - No. %02i: MOT Broadcast Web Site\n", i);
        break;
      }

      //! Transport of TPEG services
      case UAtype::TPEG:
      {
        DBGLOG(" - No. %02i: TPEG length %i\n", i, length);
        break;
      }

      //! DGPS
      //! Obsolete since ETSI TS 101 756 V2.1.1 (2017-01)
      case UAtype::DGPS:
      {
        DBGLOG(" - No. %02i: DGPS\n", i);
        break;
      }

      //! DAB-TMC (Traffic Message Channel)"
      //! Obsolete since ETSI TS 101 756 V2.1.1 (2017-01)
      case UAtype::TMC:
      {
        DBGLOG(" - No. %02i: TMC\n", i);
        break;
      }

      //! Service and Programme Information (SPI) for Hybrid Digital Radio (DAB, DRM, RadioDNS)
      case UAtype::SPI:
      {
        DBGLOG(" - No. %02i: EPG length %i\n", i, length);
        break;
      }

      //! DAB Java
      //! Obsolete since ETSI TS 101 756 V2.1.1 (2017-01)
      case UAtype::DABJava:
      {
        DBGLOG(" - No. %02i: DAB Java\n", i);
        break;
      }

      //! DMB video service
      case UAtype::DMB:
      {
        DBGLOG(" - No. %02i: DMB\n", i);
        break;
      }

      //! IPDC Services; Transport specification
      //! Obsolete since ETSI TS 101 756 V2.1.1 (2017-01)
      case UAtype::IPDCServices:
      {
        DBGLOG(" - No. %02i: IPDC services\n", i);
        break;
      }

      //! Voice Applications
      //! Obsolete since ETSI TS 101 756 V2.1.1 (2017-01)
      case UAtype::VoiceApplications:
      {
        DBGLOG(" - No. %02i: Voice applications\n", i);
        break;
      }

      //! Middleware - Part 1: System aspects
      //! Obsolete since ETSI TS 101 756 V2.1.1 (2017-01)
      case UAtype::Middleware:
      {
        DBGLOG(" - No. %02i: Middleware\n", i);
        break;
      }

      //! Filecasting
      case UAtype::Filecasting:
      {
        DBGLOG(" - No. %02i: Filecasting\n", i);
        break;
      }

      //! Filtered Information Service (FIS)
      case UAtype::FIS:
      {
        DBGLOG(" - No. %02i: FIS\n", i);
        break;
      }

      //! Journaline®
      case UAtype::Journaline:
      {
        DBGLOG(" - No. %02i: Journaline\n", i);
        break;
      }

      default:
      {
        DBGLOG(" - No. %02i: WARNING: Unsupported app type: 0x%X\n", i, appType);
        break;
      }
    }

    bitOffset = bitOffsetNext;
  }

  (void)SId;
  (void)SCIdS;
  return bitOffset / 8;
}

void FIBProcessor::FIG0Extension14(uint8_t* d)
{
  int16_t length = getBits_5(d, 3); // in Bytes
  int16_t used = 2; // in Bytes

  while (used < length)
  {
    int16_t subChId = getBits_6(d, used * 8);
    uint8_t fecScheme = getBits_2(d, used * 8 + 6);
    used = used + 1;

    for (int i = 0; i < 64; i++)
    {
      if (subChannels[i].subChId == subChId)
      {
        subChannels[i].fecScheme = fecScheme;
      }
    }
  }
}

void FIBProcessor::FIG0Extension17(uint8_t* d)
{
  int16_t length = getBits_5(d, 3);
  int16_t offset = 16;
  Service* s;

  while (offset < length * 8)
  {
    uint16_t SId = getBits(d, offset, 16);
    bool L_flag = getBits_1(d, offset + 18);
    bool CC_flag = getBits_1(d, offset + 19);
    int16_t type;
    int16_t Language = 0x00; // init with unknown language
    s = findServiceId(SId);
    if (L_flag)
    { // language field present
      Language = getBits_8(d, offset + 24);
      if (s)
      {
        s->language = Language;
      }
      offset += 8;
    }

    type = getBits_5(d, offset + 27);
    if (s)
    {
      s->programType = type;
    }
    if (CC_flag)
    { // cc flag
      offset += 40;
    }
    else
    {
      offset += 32;
    }
  }
}

void FIBProcessor::FIG0Extension18(uint8_t* d)
{
  int16_t offset = 16; // bits
  uint16_t SId, AsuFlags;
  int16_t Length = getBits_5(d, 3);

  while (offset / 8 < Length - 1)
  {
    int16_t NumClusters = getBits_5(d, offset + 35);
    SId = getBits(d, offset, 16);
    AsuFlags = getBits(d, offset + 16, 16);
    DBGLOG("fib-processor: Announcement %d for SId %d with %d clusters\n", AsuFlags, SId,
           NumClusters);
    offset += 40 + NumClusters * 8;
  }
  (void)SId;
  (void)AsuFlags;
}

void FIBProcessor::FIG0Extension19(uint8_t* d)
{
  int16_t offset = 16; // bits
  int16_t Length = getBits_5(d, 3);
  uint8_t region_Id_Lower;

  while (offset / 8 < Length - 1)
  {
    uint8_t clusterId = getBits_8(d, offset);
    bool new_flag = getBits_1(d, offset + 24);
    bool region_flag = getBits_1(d, offset + 25);
    uint8_t subChId = getBits_6(d, offset + 26);

    uint16_t aswFlags = getBits(d, offset + 8, 16);
    DBGLOG("fib-processor: %s %s Announcement %d for Cluster %2u on SubCh %2u ",
           ((new_flag == 1) ? "new" : "old"), ((region_flag == 1) ? "regional" : ""), aswFlags,
           clusterId, subChId);
    if (region_flag)
    {
      region_Id_Lower = getBits_6(d, offset + 34);
      offset += 40;
      fprintf(stderr, "for region %u", region_Id_Lower);
    }
    else
    {
      offset += 32;
    }

    fprintf(stderr, "\n");
    (void)clusterId;
    (void)new_flag;
    (void)subChId;
    (void)aswFlags;
  }
  (void)region_Id_Lower;
}

void FIBProcessor::FIG0Extension21(uint8_t* d)
{
  DBGLOG("fib-processor: Frequency information\n");
  (void)d;
}

void FIBProcessor::FIG0Extension22(uint8_t* d)
{
  int16_t Length = getBits_5(d, 3);
  int16_t offset = 16; // on bits
  int16_t used = 2;

  while (used < Length)
  {
    used = HandleFIG0Extension22(d, used);
  }
  (void)offset;
}

void FIBProcessor::FIG0ExtensionUnsupported(uint8_t* d)
{
  const uint8_t extension = getBits_5(d, 8 + 3);
  DBGLOG("FIBProcessor::FIG0ExtensionUnsupported: Not supported FIG0/%i called!\n", extension);
}

int16_t FIBProcessor::HandleFIG0Extension22(uint8_t* d, int16_t used)
{
  uint8_t MS;
  int16_t mainId;
  int16_t noSubfields;

  mainId = getBits_7(d, used * 8 + 1);
  (void)mainId;
  MS = getBits_1(d, used * 8);
  if (MS == 0)
  { // fixed size
    int16_t latitudeCoarse = getBits(d, used * 8 + 8, 16);
    int16_t longitudeCoarse = getBits(d, used * 8 + 24, 16);
    DBGLOG("fib-processor: Id = %d, (%d %d)\n", mainId, latitudeCoarse, longitudeCoarse);
    (void)latitudeCoarse;
    (void)longitudeCoarse;
    return used + 48 / 6;
  }
  //  MS == 1

  noSubfields = getBits_3(d, used * 8 + 13);
  DBGLOG("fib-processor: Id = %d, subfields = %d\n", mainId, noSubfields);
  used += (16 + noSubfields * 48) / 8;

  return used;
}

//  FIG 1 - Labels
void FIBProcessor::process_FIG1(uint8_t* d)
{
  uint32_t SId = 0;
  int16_t offset = 0;
  Service* service;
  ServiceComponent* component;
  uint8_t pd_flag;
  uint8_t SCidS;
  char label[17];

  // FIG 1 first byte
  const uint8_t charSet = getBits_4(d, 8);
  const uint8_t oe = getBits_1(d, 8 + 4);
  const uint8_t extension = getBits_3(d, 8 + 5);
  label[16] = 0x00;
  if (oe == 1)
  {
    return;
  }

  switch (extension)
  {
    case 0: // ensemble label
    {
      const uint32_t EId = getBits(d, 16, 16);
      offset = 32;
      for (int i = 0; i < 16; i++)
      {
        label[i] = getBits_8(d, offset);
        offset += 8;
      }
      DBGLOG("fib-processor: Ensemblename: %s", label);
      if (!oe and EId == ensembleId)
      {
        ensembleLabel.fig1_flag = getBits(d, offset, 16);
        ensembleLabel.fig1_label = label;
        ensembleLabel.setCharset(charSet);
        myRadioInterface.onSetEnsembleLabel(ensembleLabel);
      }
      break;
    }

    case 1: // 16 bit Identifier field for service label
      SId = getBits(d, 16, 16);
      offset = 32;
      service = findServiceId(SId);
      if (service)
      {
        for (int i = 0; i < 16; i++)
        {
          label[i] = getBits_8(d, offset);
          offset += 8;
        }
        service->serviceLabel.fig1_flag = getBits(d, offset, 16);
        service->serviceLabel.fig1_label = label;
        service->serviceLabel.setCharset(charSet);

        // MB: Added
        myRadioInterface.onSetServiceLabel(SId, service->serviceLabel);

        DBGLOG("fib-processor: FIG1/1: SId = %4x\t%s\n", SId, label);
      }
      break;

    case 3: // Region label
    {
      uint8_t region_id = getBits_6(d, 16 + 2);
      offset = 24;
      for (int i = 0; i < 16; i++)
      {
        label[i] = getBits_8(d, offset + 8 * i);
      }

      DBGLOG("fib-processor: FIG1/3: RegionID = %2x\t%s\n", region_id, label);
      break;
    }
    case 4: // Component label
      pd_flag = getBits(d, 16, 1);
      SCidS = getBits(d, 20, 4);
      if (pd_flag)
      { // 32 bit identifier field for service component label
        SId = getBits(d, 24, 32);
        offset = 56;
      }
      else
      { // 16 bit identifier field for service component label
        SId = getBits(d, 24, 16);
        offset = 40;
      }

      for (int i = 0; i < 16; i++)
      {
        label[i] = getBits_8(d, offset);
        offset += 8;
      }

      component = findComponent(SId, SCidS);
      if (component)
      {
        component->componentLabel.fig1_flag = getBits(d, offset, 16);
        component->componentLabel.setCharset(charSet);
        component->componentLabel.fig1_label = label;
      }
      DBGLOG("fib-processor: FIG1/4: Sid = %8x\tp/d=%d\tSCidS=%1X\tflag=%8X\t%s\n", SId, pd_flag,
             SCidS, getBits(d, offset, 16), label);
      break;

    case 5: // 32 bit Identifier field for service label
      SId = getBits(d, 16, 32);
      offset = 48;
      service = findServiceId(SId);
      if (service)
      {
        for (int i = 0; i < 16; i++)
        {
          label[i] = getBits_8(d, offset);
          offset += 8;
        }
        service->serviceLabel.fig1_flag = getBits(d, offset, 16);
        service->serviceLabel.fig1_label = label;
        service->serviceLabel.setCharset(charSet);

        // MB: Added
        myRadioInterface.onSetServiceLabel(SId, service->serviceLabel);

#ifdef MSC_DATA__
        myRadioInterface.onServiceDetected(SId);
#endif
      }
      break;

    case 6: // XPAD label
      uint8_t XPAD_aid;
      pd_flag = getBits(d, 16, 1);
      SCidS = getBits(d, 20, 4);
      if (pd_flag)
      { // 32 bits identifier for XPAD label
        SId = getBits(d, 24, 32);
        XPAD_aid = getBits(d, 59, 5);
        offset = 64;
      }
      else
      { // 16 bit identifier for XPAD label
        SId = getBits(d, 24, 16);
        XPAD_aid = getBits(d, 43, 5);
        offset = 48;
      }

      for (int i = 0; i < 16; i++)
      {
        label[i] = getBits_8(d, offset + 8 * i);
      }

      DBGLOG("fib-processor:"
             "FIG1/6: SId = %8x\tp/d = %d\t SCidS = %1X\tXPAD_aid = %2u\t%s\n",
             SId, pd_flag, SCidS, XPAD_aid, label);
      break;

    default:
      DBGLOG("fib-processor: FIG1/%d: not handled now\n", extension);
      break;
  }
}

static void handle_ext_label_data_field(const uint8_t* f,
                                        uint8_t len_bytes,
                                        uint8_t toggle_flag,
                                        uint8_t segment_index,
                                        uint8_t rfu,
                                        DabLabel& label)
{
  using charsets::CharacterSet;

  if (label.toggle_flag != toggle_flag)
  {
    label.segments.clear();
    label.extended_label_charset = CharacterSet::Undefined;
    label.toggle_flag = toggle_flag;
  }

  size_t len_character_field = len_bytes;

  if (segment_index == 0)
  {
    // Only if it's the first segment
    const uint8_t encoding_flag = (f[0] & 0x80) >> 7;
    const uint8_t segment_count = (f[0] & 0x70) >> 4;
    label.segment_count = segment_count + 1;

    if (encoding_flag)
    {
      label.extended_label_charset = CharacterSet::UnicodeUcs2;
    }
    else
    {
      label.extended_label_charset = CharacterSet::UnicodeUtf8;
    }

    if (rfu == 0)
    {
      // const uint8_t rfa = (f[0] & 0x0F);
      // const uint16_t char_flag = f[1] * 256 + f[2];

      if (len_bytes <= 3)
      {
        throw std::runtime_error("FIG2 label length too short");
      }

      f += 3;
      len_character_field -= 3;
    }
    else
    {
      // ETSI TS 103 176 draft V2.2.1 (2018-08) gives a new meaning to rfu
      // TODO const uint8_t text_control = (f[0] & 0x0F);

      if (len_bytes <= 1)
      {
        throw std::runtime_error("FIG2 label length too short");
      }

      f += 1;
      len_character_field -= 1;
    }

    label.fig2_rfu = rfu;
  }

  std::vector<uint8_t> labelbytes(f, f + len_character_field);
  label.segments[segment_index] = labelbytes;
}

// UTF-8 or UCS2 Labels
void FIBProcessor::process_FIG2(uint8_t* d)
{
  // In order to reuse code with etisnoop, convert
  // the bit-vector into a byte-vector
  std::vector<uint8_t> fig_bytes;
  for (size_t i = 0; i < 30; i++)
  {
    fig_bytes.push_back(getBits_8(d, 8 * i));
  }

  uint8_t* f = fig_bytes.data();

  const uint8_t figlen = f[0] & 0x1F;
  f++;

  const uint8_t toggle_flag = (f[0] & 0x80) >> 7;
  const uint8_t segment_index = (f[0] & 0x70) >> 4;
  const uint16_t rfu = (f[0] & 0x08) >> 3;
  const uint16_t ext = f[0] & 0x07;

  size_t identifier_len;
  switch (ext)
  {
    case 0: // Ensemble label
      identifier_len = 2;
      break;
    case 1: // Programme service label
      identifier_len = 2;
      break;
    case 4: // Service component label
    {
      uint8_t pd = (f[1] & 0x80) >> 7;
      identifier_len = (pd == 0) ? 3 : 5;
      break;
    }
    case 5: // Data service label
      identifier_len = 4;
      break;
    default:
      return; // Unsupported
  }

  const size_t header_length = 1; // FIG data field header

  const uint8_t* figdata = f + header_length + identifier_len;
  const size_t data_len_bytes = figlen - header_length - identifier_len;

  // ext is followed by Identifier field of Type 2 field,
  // whose length depends on ext
  switch (ext)
  {
    case 0: // Ensemble label
    { // ETSI EN 300 401 8.1.13
      uint16_t eid = f[1] * 256 + f[2];
      if (figlen <= header_length + identifier_len)
      {
        DBGLOG("FIG2/0 length error %i", (int)figlen);
      }
      else if (eid == ensembleId)
      {
        handle_ext_label_data_field(figdata, data_len_bytes, toggle_flag, segment_index, rfu,
                                    ensembleLabel);
      }
    }
    break;

    case 1: // Programme service label
    { // ETSI EN 300 401 8.1.14.1
      uint16_t sid = f[1] * 256 + f[2];
      if (figlen <= header_length + identifier_len)
      {
        DBGLOG("FIG2/1 length error %i", (int)figlen);
      }
      else
      {
        auto* service = findServiceId(sid);
        if (service)
        {
          handle_ext_label_data_field(figdata, data_len_bytes, toggle_flag, segment_index, rfu,
                                      service->serviceLabel);
        }
      }
    }
    break;

    case 4: // Service component label
    { // ETSI EN 300 401 8.1.14.3
      uint32_t sid;
      uint8_t pd = (f[1] & 0x80) >> 7;
      uint8_t SCIdS = f[1] & 0x0F;
      if (pd == 0)
      {
        sid = f[2] * 256 + f[3];
      }
      else
      {
        sid = ((uint32_t)f[2] << 24) | ((uint32_t)f[3] << 16) | ((uint32_t)f[4] << 8) |
              ((uint32_t)f[5]);
      }
      if (figlen <= header_length + identifier_len)
      {
        DBGLOG("FIG2/4 length error %i\n", (int)figlen);
      }
      else
      {
        auto* component = findComponent(sid, SCIdS);
        if (component)
        {
          handle_ext_label_data_field(figdata, data_len_bytes, toggle_flag, segment_index, rfu,
                                      component->componentLabel);
        }
      }
    }
    break;

    case 5: // Data service label
    { // ETSI EN 300 401 8.1.14.2
      const uint32_t sid = ((uint32_t)f[1] << 24) | ((uint32_t)f[2] << 16) | ((uint32_t)f[3] << 8) |
                           ((uint32_t)f[4]);

      if (figlen <= header_length + identifier_len)
      {
        DBGLOG("FIG2/5 length error %i", (int)figlen);
      }
      else
      {
        auto* service = findServiceId(sid);
        if (service)
        {
          handle_ext_label_data_field(figdata, data_len_bytes, toggle_flag, segment_index, rfu,
                                      service->serviceLabel);
        }
      }
    }
    break;
  }
}

// Conditional Access (CA)
void FIBProcessor::process_FIG6(uint8_t* d)
{
  DBGLOG("FIBProcessor::process_FIGUnsupported: Not supported FIG6 for Conditional Access (CA) "
         "called!\n");
}

// With FIG type 7 ("111") and FIG data field type 31 ("11111") used for the end marker
void FIBProcessor::process_FIG7(uint8_t* d)
{
}

void FIBProcessor::process_FIGUnsupported(uint8_t* d)
{
  const uint8_t FIGtype = getBits_3(d, 0);
  DBGLOG("FIBProcessor::process_FIGUnsupported: Not supported FIG%i called!\n", FIGtype);
}

// locate a reference to the entry for the Service serviceId
Service* FIBProcessor::findServiceId(uint32_t serviceId)
{
  for (size_t i = 0; i < services.size(); i++)
  {
    if (services[i].serviceId == serviceId)
    {
      return &services[i];
    }
  }

  return nullptr;
}

ServiceComponent* FIBProcessor::findComponent(uint32_t serviceId, int16_t SCIdS)
{
  auto comp = std::find_if(components.begin(), components.end(), [&](const ServiceComponent& sc) {
    return sc.SId == serviceId && sc.componentNr == SCIdS;
  });

  if (comp == components.end())
  {
    return nullptr;
  }
  else
  {
    return &(*comp);
  }
}

ServiceComponent* FIBProcessor::findPacketComponent(int16_t SCId)
{
  for (auto& component : components)
  {
    if (component.TMid != 03)
    {
      continue;
    }
    if (component.SCId == SCId)
    {
      return &component;
    }
  }
  return nullptr;
}

//  bindAudioService is the main processor for - what the name suggests -
//  connecting the description of audioservices to a SID
void FIBProcessor::bindAudioService(
    int8_t TMid, uint32_t SId, int16_t compnr, int16_t subChId, int16_t ps_flag, int16_t ASCTy)
{
  Service* s = findServiceId(SId);
  if (!s)
    return;

  if (std::find_if(components.begin(), components.end(), [&](const ServiceComponent& sc) {
        return sc.SId == s->serviceId && sc.componentNr == compnr;
      }) == components.end())
  {
    ServiceComponent newcomp;
    newcomp.TMid = TMid;
    newcomp.componentNr = compnr;
    newcomp.SId = SId;
    newcomp.subchannelId = subChId;
    newcomp.PS_flag = ps_flag;
    newcomp.ASCTy = ASCTy;
    components.push_back(newcomp);

    DBGLOG("fib-processor: service %8x (comp %d) is audio\n", SId, compnr);
  }
}

void FIBProcessor::bindDataStreamService(
    int8_t TMid, uint32_t SId, int16_t compnr, int16_t subChId, int16_t ps_flag, int16_t DSCTy)
{
  Service* s = findServiceId(SId);
  if (!s)
    return;

  if (std::find_if(components.begin(), components.end(), [&](const ServiceComponent& sc) {
        return sc.SId == s->serviceId && sc.componentNr == compnr;
      }) == components.end())
  {
    ServiceComponent newcomp;
    newcomp.TMid = TMid;
    newcomp.SId = SId;
    newcomp.subchannelId = subChId;
    newcomp.componentNr = compnr;
    newcomp.PS_flag = ps_flag;
    newcomp.DSCTy = DSCTy;
    components.push_back(newcomp);

    DBGLOG("fib-processor: service %8x (comp %d) is packet\n", SId, compnr);
  }
}

//      bindPacketService is the main processor for - what the name suggests -
//      connecting the service component defining the service to the SId,
///     Note that the subchannel is assigned through a FIG0/3
void FIBProcessor::bindPacketService(
    int8_t TMid, uint32_t SId, int16_t compnr, int16_t SCId, int16_t ps_flag, int16_t CAflag)
{
  Service* s = findServiceId(SId);
  if (!s)
    return;

  if (std::find_if(components.begin(), components.end(), [&](const ServiceComponent& sc) {
        return sc.SId == s->serviceId && sc.componentNr == compnr;
      }) == components.end())
  {
    ServiceComponent newcomp;
    newcomp.TMid = TMid;
    newcomp.SId = SId;
    newcomp.componentNr = compnr;
    newcomp.SCId = SCId;
    newcomp.PS_flag = ps_flag;
    newcomp.CAflag = CAflag;
    components.push_back(newcomp);

    DBGLOG("fib-processor: service %8x (comp %d) is packet\n", SId, compnr);
  }
}

void FIBProcessor::dropService(uint32_t SId)
{
  //    std::stringstream ss;
  //    ss << "Dropping service " << SId;

  services.erase(std::remove_if(services.begin(), services.end(),
                                [&](const Service& s) { return s.serviceId == SId; }),
                 services.end());

  components.erase(std::remove_if(components.begin(), components.end(),
                                  [&](const ServiceComponent& c) {
                                    const bool drop = c.SId == SId;
                                    if (drop)
                                    {
                                      //ss << ", comp " << c.componentNr;
                                    }
                                    return drop;
                                  }),
                   components.end());

  // Check for orphaned subchannels
  for (auto& sub : subChannels)
  {
    if (sub.subChId == -1)
    {
      continue;
    }

    auto c_it = std::find_if(components.begin(), components.end(), [&](const ServiceComponent& c) {
      return c.subchannelId == sub.subChId;
    });

    const bool drop = c_it == components.end();
    if (drop)
    {
      //ss << ", subch " << sub.subChId;
      sub.subChId = -1;
    }
  }

  //std::clog << ss.str() << std::endl;
}

void FIBProcessor::clearEnsemble()
{
  std::lock_guard<std::mutex> lock(mutex);
  components.clear();
  subChannels.resize(64);
  services.clear();
  serviceRepeatCount.clear();
  timeLastServiceDecrement = std::chrono::steady_clock::now();
  timeLastFCT0Frame = std::chrono::system_clock::now();
}

std::vector<Service> FIBProcessor::getServiceList() const
{
  std::lock_guard<std::mutex> lock(mutex);
  return services;
}

Service FIBProcessor::getService(uint32_t sId) const
{
  std::lock_guard<std::mutex> lock(mutex);

  auto srv = std::find_if(services.begin(), services.end(),
                          [&](const Service& s) { return s.serviceId == sId; });

  if (srv != services.end())
  {
    return *srv;
  }
  else
  {
    return Service(0);
  }
}

std::list<ServiceComponent> FIBProcessor::getComponents(const Service& s) const
{
  std::list<ServiceComponent> c;
  std::lock_guard<std::mutex> lock(mutex);
  for (const auto& component : components)
  {
    if (component.SId == s.serviceId)
    {
      c.push_back(component);
    }
  }

  return c;
}

Subchannel FIBProcessor::getSubchannel(const ServiceComponent& sc) const
{
  std::lock_guard<std::mutex> lock(mutex);
  return subChannels.at(sc.subchannelId);
}

uint16_t FIBProcessor::getEnsembleId() const
{
  std::lock_guard<std::mutex> lock(mutex);
  return ensembleId;
}

uint8_t FIBProcessor::getEnsembleEcc() const
{
  std::lock_guard<std::mutex> lock(mutex);
  return ensembleEcc;
}

DabLabel FIBProcessor::getEnsembleLabel() const
{
  std::lock_guard<std::mutex> lock(mutex);
  return ensembleLabel;
}

std::chrono::system_clock::time_point FIBProcessor::getTimeLastFCT0Frame() const
{
  std::lock_guard<std::mutex> lock(mutex);
  return timeLastFCT0Frame;
}
