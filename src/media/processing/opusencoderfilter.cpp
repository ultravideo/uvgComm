#include "opusencoderfilter.h"

#include "statisticsinterface.h"
#include "src/media/resourceallocator.h"

#include "common.h"
#include "settingskeys.h"
#include "global.h"
#include "logger.h"

#include <QDateTime>
#include <QSettings>


OpusEncoderFilter::OpusEncoderFilter(QString id, QAudioFormat format,
                                     StatisticsInterface* stats,
                                     std::shared_ptr<ResourceAllocator> hwResources):
  Filter(id, "Opus Encoder", stats, hwResources, DT_RAWAUDIO, DT_OPUSAUDIO),
  enc_(nullptr),
  opusOutput_(nullptr),
  max_data_bytes_(65536),
  format_(format),
  samplesPerFrame_(0)
{
  opusOutput_ = new uchar[max_data_bytes_];
}


OpusEncoderFilter::~OpusEncoderFilter()
{
  opus_encoder_destroy(enc_);
  enc_ = nullptr;
  delete[] opusOutput_;
  opusOutput_ = nullptr;
}


bool OpusEncoderFilter::init()
{
  int error = 0;
  enc_ = opus_encoder_create(format_.sampleRate(), format_.channelCount(), OPUS_APPLICATION_VOIP, &error);

  if(error)
  {
    Logger::getLogger()->printWarning(this, "Failed to initialize opus encoder.",
                                      {"Errorcode"}, {QString::number(error)});
    return false;
  }

  samplesPerFrame_ = format_.sampleRate()/AUDIO_FRAMES_PER_SECOND;

  updateSettings();

  return true;
}


void OpusEncoderFilter::updateSettings()
{
  QSettings settings(settingsFile, settingsFileFormat);

  int bitrate = settings.value(SettingsKey::audioBitrate).toInt();
  int complexity = settings.value(SettingsKey::audioComplexity).toInt();
  QString type = settings.value(SettingsKey::audioSignalType).toString();

  opus_encoder_ctl(enc_, OPUS_SET_BITRATE(bitrate));
  opus_encoder_ctl(enc_, OPUS_SET_COMPLEXITY(complexity));

  if (type == "Auto")
  {
    opus_encoder_ctl(enc_, OPUS_SET_SIGNAL(OPUS_AUTO));
  }
  else if (type == "Voice")
  {
    opus_encoder_ctl(enc_, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
  }
  else if (type == "Music")
  {
    opus_encoder_ctl(enc_, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));
  }
  else
  {
    Logger::getLogger()->printWarning(this, "Incorrect value in settings");
  }

  Filter::updateSettings();
}


void OpusEncoderFilter::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    // The audiocapturefilter makes sure the frames are the correct (samplesPerFrame_) size.
    if (input->data_size != samplesPerFrame_*format_.bytesPerFrame())
    {
      Logger::getLogger()->printProgramError(this, "Wrong size of input frame");
      return;
    }

    opus_int32 len = 0; // encoded frame size
    uint32_t pos = 0; // output position TODO: Is this pos variable necessary?

    opus_encoder_ctl(enc_, OPUS_SET_BITRATE(getHWManager()->getBitrate(outputType())));

    // The audiocapturefilter makes sure the frames are the samplesPerFrame size.

    len = opus_encode(enc_, (opus_int16*)input->data.get(), samplesPerFrame_,
                      opusOutput_ + pos, max_data_bytes_ - pos);
    if(len <= 0)
    {
      Logger::getLogger()->printWarning(this,  "Failed to encode audio",
        {"Errorcode:"}, {QString::number(len)});
      break;
    }

    std::unique_ptr<Data> u_copy(shallowDataCopy(input.get()));

    std::unique_ptr<uchar[]> opus_frame(new uchar[len]);
    memcpy(opus_frame.get(), opusOutput_ + pos, len);
    u_copy->data_size = len;

    u_copy->data = std::move(opus_frame);
    sendOutput(std::move(u_copy));

    /*Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Encoded Opus Audio.",
              {"Input size", "Index", "Position", "Output size"},
              {QString::number(input->data_size), QString::number(i),
               QString::number(pos), QString::number(len)});*/
    pos += len;

    if(len > 0)
    {
      uint32_t delay = QDateTime::currentMSecsSinceEpoch() - input->creationTimestamp;
      
      getStats()->encodingDelay("audio", delay);
      getStats()->addEncodedPacket("audio", len);
    }
    else
    {
      Logger::getLogger()->printWarning(this,  "Failed to encode audio frame.",
        {"Frame length"}, {QString::number(input->data_size)});
    }
    input = getInput();
  }
}
