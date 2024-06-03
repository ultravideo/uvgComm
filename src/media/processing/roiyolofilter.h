#pragma once

#include "detection_types.h"
#include "filter.h"
#include "global.h"

#include <onnxruntime/core/session/onnxruntime_cxx_api.h>

#include <array>

#ifdef KVAZZUP_HAVE_OPENCV
#include <opencv2/core.hpp>
#endif

struct Size {
  int width;
  int height;
};

struct RoiSettings {
  int width = 0;
  int height = 0;
  int backgroundQP = 0;
  int roiQP = -20;
  int qp = 27;

#ifdef KVAZZUP_HAVE_OPENCV
  cv::Mat kernel;
#endif
};

class VideoInterface;

class ROIYoloFilter : public Filter {
public:
  ROIYoloFilter(QString id, StatisticsInterface* stats,
            std::shared_ptr<ResourceAllocator> hwResources,
            bool cuda,
            VideoInterface* roiInterface);

  ~ROIYoloFilter();

  virtual bool init() override;

  void updateSettings() override;

protected:
  void process() override;

private:

  bool initYolo(int threads, QString newModelQstr);

  void close();

  void YUVToFloat(uint8_t* src, float* dst, size_t len);
  std::vector<Detection> yolo_detection(const Data* input);

  Rect find_largest_bbox(std::vector<Detection> &detections);
  std::vector<float> scaleToLetterbox(const std::vector<float>& img, Size original_shape, Size new_shape, uint8_t color = 114,
                               bool minimum = false, bool scaleFill = false, bool scaleup = true);

  std::vector<const float*> non_max_suppression_obj(Ort::Value const &prediction, bool faceDetection,
          double conf_thres=0.25,
          double iou_thres=0.45);

  std::vector<Detection> scale_coords(Size img1_shape, std::vector<const float *> const &coords,
                                      Size img0_shape);
  void clip_coords(std::vector<Rect> &boxes, Size img_shape);
  bool filter_bb(Rect bb, Size min_size);
  Rect bbox_to_roi(Rect bb);
  Size calculate_roi_size(uint32_t img_width, uint32_t img_height);
  Rect enlarge_bb(Detection face);
  Ort::SessionOptions get_session_options(bool cuda, int threads);
  RoiMap makeRoiMap(const std::vector<Rect> &bbs);

  unsigned int prevInputDiscarded_;
  unsigned int skipInput_;

#ifdef _WIN32
  std::wstring yoloModelPath_;
#else
  std::string modelPath_;
#endif

  struct ModelData
  {
    std::string modelPath = "";
    std::unique_ptr<Ort::Session> session = nullptr;
    std::unique_ptr<Ort::Allocator> allocator = nullptr;

    int inputSize = -1;

    std::optional<Ort::AllocatedStringPtr> inputName;
    std::vector<int64_t> inputShape;
    std::optional<Ort::AllocatedStringPtr> outputName;
    std::vector<int64_t> outputShape;

    bool minimum = false;
  };

  Ort::Env onnxEnv_;

  ModelData yoloModel_;

  Size minBbSize_;
  bool drawBbox_;
  double minRelativeBbSize_;
  bool faceDetection_;

#ifdef KVAZZUP_HAVE_OPENCV
  std::string kernelType_;
  int kernelSize_;
  cv::Mat filterKernel_;
#endif

  bool useCuda_;
  QAtomicInt roiEnabled_;
  int frameCount_;
  RoiMap roi_;
  RoiSettings roiSettings_;

  QMutex settingsMutex_;
  VideoInterface* roiSurface_;
};
