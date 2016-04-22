#pragma once


#include "filter.h"
#include <kvazaar.h>

enum RETURN_STATUS {C_SUCCESS = 0, C_FAILURE = -1};



class KvazaarFilter : public Filter
{
public:
  KvazaarFilter();

  int init(unsigned int width,
           unsigned int height,
           int32_t framerate_num,
           int32_t framerate_denom,
           float target_bitrate);

  void close();

protected:
  virtual void process();

private:
  const kvz_api *api_;
  kvz_config *config_;
  kvz_encoder *enc_;
};
