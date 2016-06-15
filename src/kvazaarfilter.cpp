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
  qDebug() << "KvazF: Iniating";

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
  config_->width = width;
  config_->height = height;
  config_->threads = 4;
  config_->qp = 37;
  //config_->target_bitrate = target_bitrate;

  enc_ = api_->encoder_open(config_);

  if(!enc_)
  {
    qCritical() << "KvazF: Failed to open Kvazaar encoder";
    return C_FAILURE;
  }

  f = fopen("kvazaar.265", "ab+");

  qDebug() << "KvazF: iniation success";
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

  qDebug() << "KvazF: encoding frame";

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

    if(!input_pic)
    {
      qCritical() << "KvazF: Failed to allocate picture";
      break;
    }

    memcpy(input_pic->y,
           input->data.get(),
           input->width*input->height);
    memcpy(input_pic->u,
           &(input->data.get()[input->width*input->height]),
           input->width*input->height/4);
    memcpy(input_pic->v,
           &(input->data.get()[input->width*input->height + input->width*input->height/4]),
           input->width*input->height/4);


    api_->encoder_encode(enc_, input_pic,
                         &data_out, &len_out,
                         &recon_pic, NULL,
                         &frame_info );

    putOutput(std::move(input));

    if(data_out != NULL)
    {


      uint64_t output_size = 0;
      for (kvz_data_chunk *chunk = data_out;
           chunk != NULL;
           chunk = chunk->next) {
        fwrite(chunk->data, 1, chunk->len, f);
        output_size += chunk->len;
      }

           qDebug() << "KvazF: frame encoded. Length: " << output_size <<
                       " POC: " << frame_info.poc;


/*
      for (kvz_data_chunk *chunk = chunks_out;
           chunk != NULL;
           chunk = chunk->next) {
        if (fwrite(chunk->data, sizeof(uint8_t), chunk->len, output) != chunk->len) {
          fprintf(stderr, "Failed to write data to file.\n");
          api->picture_free(cur_in_img);
          api->chunk_free(chunks_out);
          goto exit_failure;
        }
        written += chunk->len;
      }
      putOutput();*/
    }
  }
  input = getInput();

  qDebug() << "KvazF: input buffer empty";
}
