#ifndef ROIFILTER_H
#define ROIFILTER_H

#include "filter.h"
#include <onnxruntime/core/session/onnxruntime_cxx_api.h>

#include <array>

#ifdef HAVE_OPENCV
#include <opencv2/core.hpp>
#endif

struct Point {
  int x, y;
};

struct Size {
  int width, height;
};

struct Rect {
  int x, y, width, height;
};

struct Roi {
  int width;
  int height;
  std::unique_ptr<int8_t[]> data;
};

struct Detection
{
  Rect bbox;
  std::array<Point, 5> landmarks;
};

struct RoiMapFilter {
  Roi makeRoiMap(const std::vector<Rect> &bbs);

  std::vector<Roi> roi_maps;
  int depth = 1;
  int width = 0;
  int height = 0;
  int backgroundQP = 0;
  int roiQP = -20;
  int qp = 27;

#ifdef HAVE_OPENCV
  cv::Mat kernel;
#endif
};

class RoiFilter : public Filter {
public:
  RoiFilter(QString id, StatisticsInterface* stats,
            std::shared_ptr<ResourceAllocator> hwResources,
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


  Size minBbSize_;
  bool drawBbox_;
  double minRelativeBbSize_;

#ifdef HAVE_OPENCV
  cv::Mat filterKernel_;
#endif

  bool useCuda_;
  QAtomicInt roiEnabled_;
  int frameCount_;
  Roi roi_;
  RoiMapFilter roiFilter_;

  QMutex settingsMutex_;
};

#endif // ROIFILTER_H
