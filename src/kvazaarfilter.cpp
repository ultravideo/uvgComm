#include "kvazaarfilter.h"

#include <kvazaar.h>

#include <QtDebug>





KvazaarFilter::KvazaarFilter()
{

}

int KvazaarFilter::init(unsigned int width,
         unsigned int height,
         int32_t framerate_num,
         int32_t framerate_denom,
         float target_bitrate)
{
  api_ = kvz_api_get(8);

  config_ = api_->config_alloc();
  enc_ = NULL;

  if(!config_)
  {
    qCritical() << "KvazF:Failed to allocate Kvazaar config";
    return C_FAILURE;
  }

  api_->config_init(config_);
  config_->width = width;
  config_->height = height;
  config_->target_bitrate = target_bitrate;

  enc_ = api_->encoder_open(config_);

  if(!enc_)
  {
    qCritical() << "KvazF: Failed to open Kvazaar encoder";
    return C_FAILURE;
  }

  return C_SUCCESS;
}

void KvazaarFilter::close()
{
  if(api_)
  {
    api_->encoder_close(enc_);
    api_->config_destroy(config_);
    enc_ = NULL;
    config_ = NULL;
  }

}

void KvazaarFilter::process()
{
  Q_ASSERT(enc_);
  Q_ASSERT(config_);

  std::unique_ptr<Data> input = getInput();
  while(input)
  {
    kvz_picture *input_pic = NULL;
    kvz_picture *recon_pic = NULL;
    kvz_frame_info frame_info;
    kvz_data_chunk *data_out = NULL;
    uint32_t len_out = 0;

    if(config_->width != input->width
       || config_->height != input->height)
    {
      qCritical() << "KvazF: Unhandled change of resolution";
      break;
    }
    input_pic = api_->picture_alloc(input->width, input->height);

    api_->encoder_encode(enc_, input_pic,
                         &data_out, &len_out,
                         &recon_pic, NULL,
                         &frame_info );
  }
  input = getInput();
}
