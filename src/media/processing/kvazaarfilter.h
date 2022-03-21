#pragma once
#include "filter.h"

#include <QSize>
#include <QSettings>

struct kvz_api;
struct kvz_config;
struct kvz_encoder;
struct kvz_picture;
struct kvz_data_chunk;

class KvazaarFilter : public Filter
{
public:
  KvazaarFilter(QString id, StatisticsInterface* stats,
                std::shared_ptr<ResourceAllocator> hwResources);

  virtual void updateSettings();

  virtual bool init();

  void close();

protected:
  virtual void process();

private:

  void customParameters(QSettings& settings);

  // copy the frame data to kvazaar input in suitable format.
  void feedInput(std::unique_ptr<Data> input);

  // parse the encoded frame and send it forward.
  void parseEncodedFrame(kvz_data_chunk *data_out, uint32_t len_out,
                         kvz_picture *recon_pic);

  void sendEncodedFrame(std::unique_ptr<Data> input,
                        std::unique_ptr<uchar[]> hevc_frame,
                        uint32_t dataWritten);

  void convertFramerate(double framerate);

  const kvz_api *api_;
  kvz_config *config_;
  kvz_encoder *enc_;

  int64_t pts_;

  kvz_picture *input_pic_;

  int32_t framerate_num_;
  int32_t framerate_denom_;

  // temporarily store frame data during encoding
  std::deque<std::unique_ptr<Data>> encodingFrames_;
};
