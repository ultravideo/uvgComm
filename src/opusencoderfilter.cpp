#include "opusencoderfilter.h"

#include <QDebug>
#include <QDateTime>

#include "statisticsinterface.h"

// this is how many frames the audio capture seems to send
const uint16_t FRAMESPERSECOND = 25;

OpusEncoderFilter::OpusEncoderFilter(QString id, StatisticsInterface* stats):
  Filter(id, "Opus_Encoder", stats, true, true),
  enc_(0),
  opusOutput_(0),
  max_data_bytes_(65536),
  format_(),
  numberOfSamples_(0)
{
  opusOutput_ = new uchar[max_data_bytes_];
}

OpusEncoderFilter::~OpusEncoderFilter()
{
  opus_encoder_destroy(enc_);
  enc_ = 0;
  delete opusOutput_;
  opusOutput_ = 0;
}

void OpusEncoderFilter::init(QAudioFormat format)
{
  int error = 0;
  enc_ = opus_encoder_create(format.sampleRate(), format.channelCount(), OPUS_APPLICATION_VOIP, &error);

  if(error)
    qWarning() << "Failed to initialize opus encoder with errorcode:" << error;

  format_ = format;
  numberOfSamples_ = format_.sampleRate()/FRAMESPERSECOND;
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
    for(uint32_t i = 0; i < input->data_size; i += format_.bytesPerFrame()*numberOfSamples_)
    {
      //qDebug() << "Audio input datasize:" << input->data_size << "index:" << i << "pos:" << pos;

      len = opus_encode(enc_, (opus_int16*)input->data.get()+i/2, numberOfSamples_, opusOutput_ + pos, max_data_bytes_ - pos);
      if(len <= 0)
      {
        qWarning() << "Warning: Failed to encode audio. Error:" << len;
        break;
      }

      std::unique_ptr<Data> u_copy(shallowDataCopy(input.get()));

      std::unique_ptr<uchar[]> opus_frame(new uchar[len]);
      memcpy(opus_frame.get(), opusOutput_ + pos, len);
      u_copy->data_size = len;

      u_copy->data = std::move(opus_frame);
      sendOutput(std::move(u_copy));

      pos += len;
      //qDebug() << "Encoded Opus audio. New framesize:" << len;
    }

    if(len > 0)
    {
      uint32_t delay = QDateTime::currentMSecsSinceEpoch() -
          (input->presentationTime.tv_sec * 1000 + input->presentationTime.tv_usec/1000);

      stats_->sendDelay("audio", delay);
      stats_->addEncodedPacket("audio", len);
    }
    else
    {
      qWarning() << "Warning: Failed to encode audio frame. Length:" << input->data_size;
    }
    input = getInput();
  }
}
