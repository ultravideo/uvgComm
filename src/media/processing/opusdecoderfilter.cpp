#include "opusdecoderfilter.h"

#include "statisticsinterface.h"

#include "common.h"

OpusDecoderFilter::OpusDecoderFilter(uint32_t sessionID, QAudioFormat format, StatisticsInterface *stats):
  Filter(QString::number(sessionID), "Opus Decoder", stats, OPUSAUDIO, RAWAUDIO),
  dec_(nullptr),
  pcmOutput_(nullptr),
  max_data_bytes_(65536),
  format_(format),
  sessionID_(sessionID)
{
  pcmOutput_ = new int16_t[max_data_bytes_];
}


OpusDecoderFilter::~OpusDecoderFilter()
{
  if(pcmOutput_)
  {
    delete pcmOutput_;
  }
  pcmOutput_ = nullptr;
}


bool OpusDecoderFilter::init()
{
  int error = 0;
  dec_ = opus_decoder_create(format_.sampleRate(), format_.channelCount(), &error);

  if(error)
  {
    printWarning(this, "Failed to initialize opus decoder.",
      {"Errorcode"}, {QString::number(error)});
    return false;
  }
  return true;
}


void OpusDecoderFilter::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    getStats()->addReceivePacket(sessionID_, "Audio", input->data_size);

    // TODO: get number of channels from opus sample: opus_packet_get_nb_channels
    int32_t len = 0;
    int frame_size = max_data_bytes_/(format_.channelCount()*sizeof(opus_int16));

    len = opus_decode(dec_, input->data.get(), input->data_size, pcmOutput_, frame_size, 0);

    uint32_t datasize = len*format_.channelCount()*sizeof(opus_int16);

    //printDebug(DEBUG_NORMAL, this, "Decoded Opus audio.", {"Input size", "Output size"},
              //{QString::number(input->data_size), QString::number(datasize)};

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
      printWarning(this, "Failed to decode audio frame.", {"Error"}, {QString::number(len)});
    }
    input = getInput();
  }
}
