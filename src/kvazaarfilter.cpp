#include "kvazaarfilter.h"

#include <kvazaar.h>
#include <QtDebug>



KvazaarFilter::KvazaarFilter():input_pic_(NULL)
{
name_ = "KvazF";
}

int KvazaarFilter::init(unsigned int width,
                        unsigned int height,
                        int32_t framerate_num,
                        int32_t framerate_denom,
                        float target_bitrate)
{
  qDebug() << "KvazF: Iniating";

  // input picture should not exist at this point
  Q_ASSERT(!input_pic_ && !api_);

  api_ = kvz_api_get(8);
  if(!api_)
  {
    qCritical() << "KvazF:Failed to retreive Kvazaar API";
    return C_FAILURE;
  }
  config_ = api_->config_alloc();
  enc_ = NULL;

  if(!config_)
  {
    qCritical() << "KvazF:Failed to allocate Kvazaar config";
    return C_FAILURE;
  }

  api_->config_init(config_);
  api_->config_parse(config_, "preset", "ultrafast");
  config_->width = width;
  config_->height = height;
  config_->threads = 4;
  config_->qp = 32;
  config_->wpp = 1;
  config_->intra_period = 64;



  //config_->target_bitrate = target_bitrate;

  enc_ = api_->encoder_open(config_);

  if(!enc_)
  {
    qCritical() << "KvazF: Failed to open Kvazaar encoder";
    return C_FAILURE;
  }

  //f = fopen("kvazaar.265", "ab+");

  input_pic_ = api_->picture_alloc(width, height);

  if(!input_pic_)
  {
    qCritical() << name_ << ": Could not allocate input picture";
    return C_FAILURE;
  }

  qDebug() << "KvazF: iniation success";
  return C_SUCCESS;
}

void KvazaarFilter::close()
{
  if(api_)
  {
    if(input_pic_)
      api_->picture_free(input_pic_);

    input_pic_ = NULL;
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

  qDebug() << "KvazF: encoding frame";

  std::unique_ptr<Data> input = getInput();
  while(input)
  {
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


    if(!input_pic_)
    {
      qCritical() << name_ << ": Input picture was not allocated correctly";
      break;
    }

    memcpy(input_pic_->y,
           input->data.get(),
           input->width*input->height);
    memcpy(input_pic_->u,
           &(input->data.get()[input->width*input->height]),
           input->width*input->height/4);
    memcpy(input_pic_->v,
           &(input->data.get()[input->width*input->height + input->width*input->height/4]),
           input->width*input->height/4);


    api_->encoder_encode(enc_, input_pic_,
                         &data_out, &len_out,
                         &recon_pic, NULL,
                         &frame_info );

    if(data_out != NULL)
    {

/*
      uint64_t output_size = 0;
      for (kvz_data_chunk *chunk = data_out;chunk != NULL; chunk = chunk->next)
      {
        fwrite(chunk->data, 1, chunk->len, f);
        output_size += chunk->len;
      }
*/
      qDebug() << "KvazF: frame encoded. Length: " << len_out <<
                  " POC: " << frame_info.poc;

      Data *hevc_frame = new Data;
      hevc_frame->data_size = len_out;
      hevc_frame->data = std::unique_ptr<uchar[]>(new uchar[len_out]);
      hevc_frame->width = input->width;
      hevc_frame->height = input->height;
      hevc_frame->type = HEVCVIDEO;

      uint8_t* writer = hevc_frame->data.get();

      kvz_data_chunk *previous_chunk = 0;
      for (kvz_data_chunk *chunk = data_out; chunk != NULL; chunk = chunk->next)
      {
        memcpy(writer, chunk->data, chunk->len);
        writer += chunk->len;
        previous_chunk = chunk;
      }
      api_->chunk_free(data_out);
      api_->picture_free(recon_pic);

      std::unique_ptr<Data> hevc_frame_data( hevc_frame );
      sendOutput(std::move(hevc_frame_data));
    }
    input = getInput();
  }


  qDebug() << "KvazF: input buffer empty";
}
