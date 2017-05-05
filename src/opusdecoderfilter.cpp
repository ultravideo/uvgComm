#include "opusdecoderfilter.h"

#include <QDebug>

OpusDecoderFilter::OpusDecoderFilter(QString id, StatisticsInterface *stats):
  Filter(id, "Opus_Decoder", stats, true, true),
  dec_(0),
  pcmOutput_(0),
  max_data_bytes_(65536)
{
  pcmOutput_ = new int16_t[max_data_bytes_];
}

OpusDecoderFilter::~OpusDecoderFilter()
{
  delete pcmOutput_;
  pcmOutput_ = 0;
}

void OpusDecoderFilter::init(QAudioFormat format)
{
  int error = 0;
  dec_ = opus_decoder_create(format.sampleRate(), format.channelCount(), &error);

  if(error)
    qWarning() << "Failed to initialize opus decoder with errorcode:" << error;

  format_ = format;
}

void OpusDecoderFilter::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    // TODO: get number of channels from opus sample
    //qDebug() << "Decoding:" << input->data_size;

    //int channels = 2;
    int32_t len = 0;
    int frame_size = max_data_bytes_/(format_.channelCount()*sizeof(opus_int16));

    len = opus_decode(dec_, input->data.get(), input->data_size, pcmOutput_, frame_size, 0);

    uint32_t datasize = len*format_.channelCount()*sizeof(opus_int16);

    //qDebug() << "Decoded Opus audio. New framesize:" << datasize;

    if(len > -1)
    {
      std::unique_ptr<uchar[]> pcm_frame(new uchar[datasize]);
      memcpy(pcm_frame.get(), pcmOutput_, datasize);
      input->data_size = datasize;

      input->data = std::move(pcm_frame);
      sendOutput(std::move(input));
    }
    else
    {
      qWarning() << "Warning: Failed to encode audio frame. Error:" << len;
    }
    input = getInput();
  }
}
