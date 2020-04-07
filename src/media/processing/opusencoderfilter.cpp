#include "opusencoderfilter.h"

#include "statisticsinterface.h"

#include "common.h"

#include <QDateTime>

// this is how many frames the audio capture seems to send
const uint16_t FRAMESPERSECOND = 50;

OpusEncoderFilter::OpusEncoderFilter(QString id, QAudioFormat format, StatisticsInterface* stats):
  Filter(id, "Opus Encoder", stats, RAWAUDIO, OPUSAUDIO),
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
    printWarning(this, "Failed to initialize opus encoder.",
                  {"Errorcode"}, {QString::number(error)});
    return false;
  }

  samplesPerFrame_ = format_.sampleRate()/FRAMESPERSECOND;
  return true;
}


void OpusEncoderFilter::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    opus_int32 len = 0; // encoded frame size
    uint32_t pos = 0; // output position

    // one input packet may contain more than one audio frame (usually doesn't).
    // Process them all and send one by one.

    for(uint32_t i = 0; i < input->data_size; i += format_.bytesPerFrame()*samplesPerFrame_)
    {
      len = opus_encode(enc_, (opus_int16*)input->data.get()+i/2, samplesPerFrame_,
                        opusOutput_ + pos, max_data_bytes_ - pos);
      if(len <= 0)
      {
        printWarning(this,  "Failed to encode audio",
          {"Errorcode:"}, {QString::number(len)});
        break;
      }

      std::unique_ptr<Data> u_copy(shallowDataCopy(input.get()));

      std::unique_ptr<uchar[]> opus_frame(new uchar[len]);
      memcpy(opus_frame.get(), opusOutput_ + pos, len);
      u_copy->data_size = len;

      u_copy->data = std::move(opus_frame);
      sendOutput(std::move(u_copy));

      /*printDebug(DEBUG_NORMAL, this, "Encoded Opus Audio.",
                {"Input size", "Index", "Position", "Output size"},
                {QString::number(input->data_size), QString::number(i),
                 QString::number(pos), QString::number(len)});*/
      pos += len;
    }

    if(len > 0)
    {
      uint32_t delay = QDateTime::currentMSecsSinceEpoch() -
          (input->presentationTime.tv_sec * 1000 + input->presentationTime.tv_usec/1000);

      getStats()->sendDelay("audio", delay);
      getStats()->addEncodedPacket("audio", len);
    }
    else
    {
      printWarning(this,  "Failed to encode audio frame.",
        {"Frame length"}, {QString::number(input->data_size)});
    }
    input = getInput();
  }
}
