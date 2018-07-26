#pragma once
#include "filter.h"

#include <QSize>

struct kvz_api;
struct kvz_config;
struct kvz_encoder;
struct kvz_picture;

class KvazaarFilter : public Filter
{
public:
  KvazaarFilter(QString id, StatisticsInterface* stats);

  virtual void updateSettings();

  virtual bool init();

  void close();

protected:
  virtual void process();

private:

  void sendEncodedFrame(std::unique_ptr<Data> input,
                        std::unique_ptr<uchar[]> hevc_frame,
                        uint32_t dataWritten);

  const kvz_api *api_;
  kvz_config *config_;
  kvz_encoder *enc_;

  int64_t pts_;

  kvz_picture *input_pic_;

  int32_t framerate_num_;
  int32_t framerate_denom_;

  std::deque<timeval> encodingPresentationTimes_;
};
