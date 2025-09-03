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
                std::shared_ptr<ResourceAllocator> hwResources,
                std::pair<uint16_t, uint16_t> resolution);

  void restartEncoder();

  virtual void updateSettings();

  virtual bool init();

  void close(std::pair<int, int> resolution);

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

  void createInputVector(int size);
  void cleanupInputVector();

  void addInputPic(int index);

  kvz_picture* getNextPic();

  void reInitializeKvazaar();

  void calculate_psnr(const kvz_picture *orig, const kvz_picture *recon, double& psnr_y, double& psnr_u, double& psnr_v);

  const kvz_api *api_;
  kvz_config *config_;

  std::pair<int, int> currentResolution_;
  std::map<std::pair<int, int>, kvz_encoder*> encoders_;

  int64_t pts_;

  // filter has only one thread, so no need to lock the usage of input pics
  std::vector<kvz_picture*> inputPics_;
  int nextInputPic_;

  QMutex settingsMutex_;
  struct FrameInfo
  {
    std::unique_ptr<Data> data;
    kvz_picture *inputPic;
    int8_t * roi_array;
  };

  // temporarily store frame data during encoding
  std::deque<FrameInfo> encodingFrames_;

  int timestampInterval_;

  uint64_t currentFrame_;
};
