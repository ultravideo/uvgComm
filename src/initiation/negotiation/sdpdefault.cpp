#include "sdpdefault.h"

#include "common.h"
#include "logger.h"

#include <QDateTime>

#include <set>

// Relevant RFC 8866: https://datatracker.ietf.org/doc/html/rfc8866

/* For reference on rtp payload type numbers:
 * https://www.iana.org/assignments/rtp-parameters/rtp-parameters.xhtml

 * Please prefer using dynamic payload types instead of contant ones,
 * but these are included for completeness.

 * See RFC 3551 section 6 for predefined payload types(https://datatracker.ietf.org/doc/html/rfc3551#section-6) */
const std::set<uint8_t> PREDEFINED_AUDIO_PT = {
  0,  // PCMU
  1,  // reserved (previously FS-1016 CELP in RFC)
  2,  // reserved (previously G721 or G726-32)
  3,  // GSM
  4,  // G723
  5,  // DVI4
  6,  // DVI4
  7,  // LPC
  8,  // PCMA
  9,  // G722
  10, // L16
  11, // L16
  12, // QCELP
  13, // CN
  14, // MPA
  15, // G728
  16, // DVI4
  17, // DVI4
  18, // G729
  19, // reserved (previously CN)
  20, // unassigned
  21, // unassigned
  22, // unassigned
  23, // unassigned
  33  // MP2T, both audio and video possible
};

const std::set<uint8_t> PREDEFINED_VIDEO_PT = {
  24, // unassigned
  25, // CelB
  26, // JPEG
  27, // unassigned
  28, // nv
  29, // unassigned
  30, // unassigned
  31, // H261
  32, // MPV
  33, // MP2T, both audio and video possible
  34  // H263
};


// AV = both audio and video are supported
const std::map<QString, uint32_t> DYNAMIC_VIDEO_PAYLOADTYPES ={
  {"H263-1998",     90000}, // https://datatracker.ietf.org/doc/html/rfc4629#section-8.1.1
  {"H263-2000",     90000}, // https://datatracker.ietf.org/doc/html/rfc4629#section-8.1.2
  {"H264",          90000}, // https://datatracker.ietf.org/doc/html/rfc6184#section-8.1
  {"H264-SVC",      90000}, // https://datatracker.ietf.org/doc/html/rfc6190#section-7.1
  {"H265",          90000}, // https://datatracker.ietf.org/doc/html/rfc7798#section-7.1
  {"H266",          90000}, // https://datatracker.ietf.org/doc/html/draft-ietf-avtcore-rtp-vvc-14#section-7.1
  {"theora",        90000}, // https://datatracker.ietf.org/doc/html/draft-barbato-avt-rtp-theora#section-6
  {"MP4V-ES",       90000}, // https://datatracker.ietf.org/doc/html/rfc6416#section-7.1
  {"mpeg4-generic", 90000}, // AV, https://datatracker.ietf.org/doc/html/rfc3640#section-4.1
  {"VP8",           90000}, // https://datatracker.ietf.org/doc/html/rfc7741#section-6.1
  {"VP9",           90000}, // https://datatracker.ietf.org/doc/html/draft-ietf-payload-vp9#section-7
  {"raw",           90000}, // https://datatracker.ietf.org/doc/html/rfc4175#section-6.1
  {"jpeg2000",      90000}, // https://datatracker.ietf.org/doc/html/rfc5371#section-6
  {"JPEG2000",      90000}, // https://datatracker.ietf.org/doc/html/rfc5371#section-6
  {"DV",            90000}, // AV, https://datatracker.ietf.org/doc/html/rfc6469#section-6
  {"BT656",         90000}, // https://datatracker.ietf.org/doc/html/rfc3555#section-4.2.1
  {"BMPEG",         90000}, // https://www.rfc-editor.org/rfc/rfc2343.html
  {"SMPTE292M",     90000}, // https://datatracker.ietf.org/doc/html/rfc3497#section-7
  {"MP1S",          90000}, // https://www.rfc-editor.org/rfc/rfc3555.html#section-1.1
  {"MP2P",          90000}, // https://www.rfc-editor.org/rfc/rfc3555.html#section-1.1
  {"jxsv",          90000}  // https://www.rfc-editor.org/rfc/rfc9134.html#name-media-type-registration
};

const std::map<QString, uint32_t> DYNAMIC_AUDIO_PAYLOADTYPES = {
  {"iLBC",           8000},
  {"PCMA-WB",       16000},
  {"PCMU-WB ",      16000},
  {"G718",          32000},
  {"G719",          48000},
  {"G7221",             0}, // 16000 or 32000
  {"G726-16",        8000},
  {"G726-24",        8000},
  {"G726-32",        8000},
  {"G726-40",        8000},
  {"G729D",          8000},
  {"G729E",          8000},
  {"G7291",         16000},
  {"GSM-EFR",        8000},
  {"GSM-HR-08",      8000},
  {"AMR",            8000},
  {"AMR-WB",        16000},
  {"AMR-WB+",       72000},
  {"vorbis",            0},
  {"opus",          48000}, // https://datatracker.ietf.org/doc/html/rfc7587#section-6.1
  {"speex",             0}, // 8000, 16000, or 32000
  {"mpa-robust",    90000},
  {"MP4A-LATM",     90000},
  {"mpeg4-generic", 90000}, // AV
  {"L8",                0},
  {"DAT12",             0},
  {"L16",               0},
  {"L20",               0},
  {"L24",               0},
  {"ac3",               0}, // 32000, 44100, or 48000
  {"eac3",              0}, // 32000, 44100, or 48000

  {"EVRC",           8000},
  {"EVRC0",          8000},
  {"EVRC1",          8000},

  {"EVRCB",          8000},
  {"EVRCB0",         8000},
  {"EVRCB1",         8000},

  {"EVRCWB",        16000},
  {"EVRCWB0",       16000},
  {"EVRCWB1",       16000},

  {"UEMCLIP",           0}, // 8000 or 16000
  {"ATRAC3",        44100},
  {"ATRAC-X",           0}, // 44100 or 48000
  {"ATRAC-ADVANCED-LOSSLESS", 0},

  {"DV",             90000}, // AV

  {"RED",                0},
  {"VDVI",               0},
  {"tone",            8000},
  {"telephone-event", 8000},
  {"aptx",               0},

};

const QString SESSION_NAME = "-";
const QString SESSION_DESCRIPTION = "A Kvazzup initiated communication";



void generateMedia(QString type,
                   QList<MediaInfo>& medias,
                   QList<QString>& dynamicSubtypes,
                   QList<uint8_t>& staticPayloadTypes,
                   const std::map<QString, uint32_t> &clockFrequencies);

RTPMap createMapping(uint8_t& dynamicNumber, const QString subtype,
                     const std::map<QString, uint32_t> &clockFrequencies);

std::shared_ptr<SDPMessageInfo> generateDefaultSDP(QString username, QString localAddress,
                                                   int audioStreams, int videoStreams,
                                                   QList<QString> dynamicAudioSubtypes,
                                                   QList<QString> dynamicVideoSubtypes,
                                                   QList<uint8_t> staticAudioPayloadTypes,
                                                   QList<uint8_t> staticVideoPayloadTypes)
{
  Logger::getLogger()->printNormal("SDPNegotiationHelper",
                                   "Generating new SDP message with our address",
                                   "Local address", localAddress);

  if(username == "")
  {
    Logger::getLogger()->printWarning("SDPNegotiationHelper",
                                      "Necessary info not set for SDP generation",
                                      {"username"}, {username});

    return nullptr;
  }

  std::shared_ptr<SDPMessageInfo> newInfo
      = std::shared_ptr<SDPMessageInfo> (new SDPMessageInfo);
  newInfo->version = 0;

  if (localAddress != "" && localAddress != "0.0.0.0")
  {
    generateOrigin(newInfo, localAddress, username);
    setSDPAddress(localAddress, newInfo->connection_address,
                  newInfo->connection_nettype, newInfo->connection_addrtype);
  }



  newInfo->sessionName = SESSION_NAME;
  newInfo->sessionDescription = SESSION_DESCRIPTION;

  newInfo->timeDescriptions.push_back(TimeInfo{0,0, "", "", {}});

  for (int i = 0; i < audioStreams; ++i)
  {
    generateMedia("audio", newInfo->media, dynamicAudioSubtypes,
                  staticAudioPayloadTypes,
                  DYNAMIC_AUDIO_PAYLOADTYPES);
  }

  for (int i = 0; i < videoStreams; ++i)
  {
    generateMedia("video", newInfo->media, dynamicVideoSubtypes,
                  staticVideoPayloadTypes,
                  DYNAMIC_VIDEO_PAYLOADTYPES);
  }

  return newInfo;
}


void generateOrigin(std::shared_ptr<SDPMessageInfo> sdp,
                    QString localAddress, QString username)
{
  // RFC 2327, section 6
  sdp->originator_username = username;

  // TODO: NTP timestamp is recommended (secs and picosecs since 1900)
  sdp->sess_id = QDateTime::currentMSecsSinceEpoch();
  sdp->sess_v = QDateTime::currentMSecsSinceEpoch();

  setSDPAddress(localAddress, sdp->host_address, sdp->host_nettype, sdp->host_addrtype);
}


void setSDPAddress(QString inAddress, QString& sdpAddress, QString& type, QString& addressType)
{
  sdpAddress = inAddress;
  type = "IN";

  // TODO: Improve the address detection
  if (inAddress.front() == '[')
  {
    sdpAddress = inAddress.mid(1, inAddress.size() - 2);
    addressType = "IP6";
  }
  else
  {
    addressType = "IP4";
  }
}


void generateMedia(QString type, QList<MediaInfo>& medias,
                   QList<QString> &dynamicSubtypes,
                   QList<uint8_t> &staticPayloadTypes,
                   const std::map<QString, uint32_t> &clockFrequencies)
{

  // we ignore nettype, addrtype and address, because we use a global c=
  MediaInfo media = {type, 0, "RTP/AVP", {},
                     "", "", "", "", {},"", {}, {}, {}};

  uint8_t dynamicPayloadType = 96;

  for (auto& type : dynamicSubtypes)
  {
    media.codecs.push_back(createMapping(dynamicPayloadType, type, clockFrequencies));
  }

  for(RTPMap& codec : media.codecs)
  {
    media.rtpNums.push_back(codec.rtpNum);
  }

  // just for completeness, we will probably never support any of the pre-set video types.
  media.rtpNums += staticPayloadTypes;
  medias.push_back(media);
}


RTPMap createMapping(uint8_t& dynamicNumber, const QString subtype,
                     const std::map<QString, uint32_t> &clockFrequencies)
{
  if (dynamicNumber < 96 || dynamicNumber > 127)
  {
    Logger::getLogger()->printProgramError("SDP Default",
                                           "Invalid dynamic payload number, "
                                           "must be between 96 and 127",
                                           "Number", QString::number(dynamicNumber));
  }

  uint32_t frequency = 0;

  if (clockFrequencies.find(subtype) == clockFrequencies.end())
  {
    Logger::getLogger()->printWarning("SDP Default", "Unsupported subtype (make sure spelling is correct), "
                                                     "please manually set clock frequency");
  }
  else if (clockFrequencies.at(subtype) == 0)
  {
    Logger::getLogger()->printWarning("SDP Default",
                                      "The subtype frequency is not clearly defined, please manually set clock frequency");
  }
  else
  {
    frequency = clockFrequencies.at(subtype);
  }

  // TODO: Add codec parameter support
  RTPMap map = {dynamicNumber, frequency, subtype, ""};
  ++dynamicNumber;
  return map;
}
