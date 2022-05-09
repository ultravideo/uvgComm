#ifndef ROIFILTER_H
#define ROIFILTER_H

#include "filter.h"
#include <onnxruntime/core/session/onnxruntime_cxx_api.h>

#include <array>
#include <opencv2/core.hpp>

struct Detection
{
  cv::Rect bbox;
  std::array<cv::Point, 5> landmarks;
};

struct RoiMapFilter {
  cv::Mat makeRoiMap(const std::vector<cv::Rect> &bbs);

  cv::Mat kernel;
  std::vector<cv::Mat> roi_maps;
  int depth = 1;
  int width = 0;
  int height = 0;
  int backgroundQP = 0;
  int roiQP = -20;
  int qp = 27;
};

class RoiFilter : public Filter {
public:
  RoiFilter(QString id, StatisticsInterface* stats,
            std::shared_ptr<HWResourceManager> hwResources,
            bool cuda);

  ~RoiFilter();
  void updateSettings() override;

protected:
  void process() override;

private:
  bool init() override;
  void close();

  std::vector<Detection> detect(const Data* input);

  unsigned int prevInputDiscarded_;
  unsigned int skipInput_;

  std::wstring model_;
  std::string kernelType_;
  int kernelSize_;
  int filterDepth_;

  Ort::Env env_;
  std::unique_ptr<Ort::Session> session_;
  std::unique_ptr<Ort::Allocator> allocator_;

  char *inputName_;
  std::vector<int64_t> inputShape_;
  int inputSize_;
  bool minimum_;

  char* outputName_;
  std::vector<int64_t> outputShape_;


  cv::Size minBbSize_;
  bool drawBbox_;
  double minRelativeBbSize_;
  cv::Mat filterKernel_;

  bool useCuda_;
  QAtomicInt roiEnabled_;
  int frameCount_;
  struct {
    int width;
    int height;
    std::unique_ptr<int8_t[]> data;
  } roi_;
  RoiMapFilter roiFilter_;

  QMutex settingsMutex_;
};

#endif // ROIFILTER_H
