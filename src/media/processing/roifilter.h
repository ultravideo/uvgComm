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
  int depth;
  int width = 0;
  int height = 0;
  int backgroundQP = 0;
  int dQP = -20;
};

class RoiFilter : public Filter {
public:
  RoiFilter(QString id, QString name, StatisticsInterface* stats,
            std::shared_ptr<HWResourceManager> hwResources,
            std::wstring model, int size, bool cuda);

  ~RoiFilter();
  void updateSettings() override;

protected:
  void process() override;

private:
  bool init() override;
  void close();

  std::vector<Detection> detect(const Data* input);

  std::wstring model;
  std::string kernel_type;
  int kernel_size;
  int filter_depth;

  Ort::Env env;
  std::unique_ptr<Ort::Session> session;
  std::unique_ptr<Ort::Allocator> allocator;

  char *input_name;
  std::vector<int64_t> input_shape;
  int input_size;
  bool minimum;

  char* output_name;
  std::vector<int64_t> output_shape;


  cv::Size min_bb_size;
  bool draw_bbox;
  double min_relative_bb_size;
  cv::Mat filter_kernel;

  bool use_cuda;
  bool is_ok;
  int frame_count;
  struct {
    int width;
    int height;
    std::unique_ptr<int8_t[]> data;
  } roi;
  RoiMapFilter filter;
};

#endif // ROIFILTER_H
