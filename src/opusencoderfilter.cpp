#include "opusencoderfilter.h"

#include <QDebug>

OpusEncoderFilter::OpusEncoderFilter(StatisticsInterface* stats):
  Filter("Opus Encoder", stats, false, false),
  enc_(0),
  output_(0),
  max_data_bytes_(65536)
{
  output_ = new uchar[max_data_bytes_];
}

void OpusEncoderFilter::init()
{
  int* error = 0;
  enc_ = opus_encoder_create(48000, 2, OPUS_APPLICATION_VOIP, error);
}

void OpusEncoderFilter::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    int channels = 2;

    opus_int32 len = 0;
    int frame_size = input->data_size/(channels*sizeof(opus_int16));
    opus_int16* input_data = (opus_int16*)input->data.get();

    len = opus_encode(enc_, input_data, frame_size, output_, max_data_bytes_);

    qDebug() << "Encoded Opus audio. New framesize:" << len;

    if(len != -1)
    {
      std::unique_ptr<uchar[]> opus_frame(new uchar[len]);
      memcpy(opus_frame.get(), output_, len);
      input->data_size = len;

      input->data = std::move(opus_frame);
      sendOutput(std::move(input));
    }
    else
    {
      qWarning() << "Warning: Failed to encode audio frame";
    }
    input = getInput();
  }

}
