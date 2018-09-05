#include "sdpparametermanager.h"

SDPParameterManager::SDPParameterManager()
{}



// for reference on rtp payload type numbers:
// https://en.wikipedia.org/wiki/RTP_audio_video_profile

QList<uint8_t> SDPParameterManager::audioPayloadTypes()
{
  // TODO: payload type 0 should always be supported!
  return QList<uint8_t>{};
}

QList<uint8_t> SDPParameterManager::videoPayloadTypes()
{
  return QList<uint8_t>{};
}

QList<RTPMap> SDPParameterManager::audioCodecs() const
{
  // currently we just use a different dynamic rtp payload number for each codec
  // to make implementation simpler ( range 96...127 )

  // opus is always 48000, even if the actual sample rate is lower
  return QList<RTPMap>{RTPMap{96, 48000, "opus"}};
}

QList<RTPMap> SDPParameterManager::videoCodecs() const
{
  return QList<RTPMap>{RTPMap{97, 90000, "h265"}};
}

QString SDPParameterManager::sessionName() const
{
  return " ";
}

QString SDPParameterManager::sessionDescription() const
{
  return "A Kvazzup Video Conference";
}


