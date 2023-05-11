#include "roifilter.h"

#include "logger.h"

#include "settingskeys.h"

#include "media/resourceallocator.h"

#ifdef KVAZZUP_HAVE_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#endif

#ifdef max
#undef max
#endif

namespace
{
Rect find_largest_bbox(std::vector<Detection> &detections)
{
  Rect largest;
  int largest_area = 0;
  for (auto &d : detections)
  {
    int area = d.bbox.height * d.bbox.width;
    if (area > largest_area)
    {
      largest_area = area;
      largest = d.bbox;
    }
  }
  return largest;
}

std::vector<float> letterbox(const std::vector<float>& img, Size original_shape, Size new_shape, uint8_t color = 114,
                  bool minimum = false, bool scaleFill = false, bool scaleup = true)
{
  //Resize image to a 32 - pixel - multiple rectangle https://github.com/ultralytics/yolov3/issues/232
  // Scale ratio(new / old)
  double r = std::min((double)new_shape.width / (double)original_shape.width,
                      (double)new_shape.height / (double)original_shape.height);
  if (!scaleup) // only scale down, do not scale up (for better test mAP)
  {
    r = std::min(r, 1.0);
  }
  // Compute padding
  Size new_unpad{int(std::round(original_shape.width * r)), int(std::round(original_shape.height * r))};
  int dw = new_shape.width - new_unpad.width;
  int dh = new_shape.height - new_unpad.height; // wh padding
  if (minimum)                                  //minimum rectangle
  {
    dw = dw % 64;
    dh = dh % 64; //wh padding
  }
  else if (scaleFill) //stretch
  {
    dw = 0;
    dh = 0;
    new_unpad = {new_shape.width, new_shape.height};
  }

  double ddh = (double)dh / 2.0; // divide padding into 2 sides
  double ddw = (double)dw / 2.0;
  int top = int(std::round(ddh - 0.1));
  int bottom = int(std::round(ddh + 0.1));
  int left = int(std::round(ddw - 0.1));
  int right = int(std::round(ddw + 0.1));

#ifdef KVAZZUP_HAVE_OPENCV
  cv::Mat scaled;
#else
  std::vector<float> scaled_img((dw+new_unpad.width)*(dh+new_unpad.height), color/255.0f);
#endif

  if (original_shape.width != new_unpad.width && original_shape.height != new_unpad.height) // resize
  {
#ifdef KVAZZUP_HAVE_OPENCV
    cv::Mat input(original_shape.height, original_shape.width, CV_32F, (void*)img.data());
    cv::resize(input, scaled, {new_unpad.width, new_unpad.height}, 0.0, 0.0, cv::INTER_NEAREST);
#else
    double fx = 1/((double)new_shape.width / (double)original_shape.width);
    double fy = 1/((double)new_shape.height / (double)original_shape.height);
    for(int y = top; y < new_unpad.height+top; y++) {
      for(int x = left; x < new_unpad.width+left; x++) {
        int old_x = std::floor(x*fx),
            old_y = std::floor(y*fy);
        scaled_img[y*(new_unpad.width+dw)+x] = img[old_y*original_shape.width+old_x];
      }
    }
#endif
  }

#ifdef KVAZZUP_HAVE_OPENCV
  cv::copyMakeBorder(scaled, scaled, top, bottom, left, right, cv::BORDER_CONSTANT, color/255.0f); // add border
  std::vector<float> scaled_img(new_shape.width*new_shape.height);
  memcpy((void*)scaled_img.data(), (void*)scaled.datastart, new_shape.width*new_shape.height*scaled.elemSize());
#endif
  return scaled_img;
}

std::vector<const float *> non_max_suppression_face(Ort::Value const &prediction, double conf_thres = 0.25, double iou_thres = 0.45)
{
  assert(prediction.GetTensorTypeAndShapeInfo().GetElementType() == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT);
  assert(!prediction.IsSparseTensor());
  assert(prediction.IsTensor());

  auto shape = prediction.GetTensorTypeAndShapeInfo().GetShape();
  assert(shape.size() == 3 && shape[0] == 1 && shape[2] == 16);

  const float *data = prediction.GetTensorData<float>();
  int64_t row_size = shape[2];

  std::vector<const float *> candidates;
  for (int64_t i = 0; i < shape[1]; i++)
  {
    const float *row = data + i * row_size;
    if (row[4] > conf_thres)
    {
      candidates.push_back(row);
    }
  }
  std::vector<size_t> order(candidates.size());
  std::iota(order.begin(), order.end(), 0);
  std::sort(order.begin(), order.end(),
            [&candidates](size_t i1, size_t i2)
  {
    return candidates[i1] < candidates[i2];
  });
  std::vector<uint8_t> suppressed(candidates.size(), 0);
  std::vector<size_t> keep(candidates.size(), 0);
  size_t num_to_keep = 0;

  // Settings
  // (pixels) minimum and maximum box width and height
  std::vector<Rect> output;
  for (size_t i_ = 0; i_ < candidates.size(); i_++)
  {
    auto i = order[i_];
    if (suppressed[i] == 1)
    {
      continue;
    }
    keep[num_to_keep++] = i;
    const float *x = candidates[i];
    auto ix1 = x[0] - x[2] / 2;
    auto iy1 = x[1] - x[3] / 2;
    auto ix2 = x[0] + x[2] / 2;
    auto iy2 = x[1] + x[3] / 2;
    auto iarea = (ix2 - ix1) * (iy2 - iy1);
    //cv::Rect2f box(x[0] - x[2] / 2, x[1] - x[3] / 2, x[2], x[3]);

    for (size_t j_ = i_ + 1; j_ < candidates.size(); j_++)
    {
      auto j = order[j_];
      if (suppressed[j] == 1)
      {
        continue;
      }

      const float *xj = candidates[j];
      auto jx1 = xj[0] - xj[2] / 2;
      auto jy1 = xj[1] - xj[3] / 2;
      auto jx2 = xj[0] + xj[2] / 2;
      auto jy2 = xj[1] + xj[3] / 2;
      auto jarea = (jx2 - jx1) * (jy2 - jy1);

      auto xx1 = std::max(ix1, jx1);
      auto yy1 = std::max(iy1, jy1);
      auto xx2 = std::min(ix2, jx2);
      auto yy2 = std::min(iy2, jy2);

      auto w = std::max(0.0f, xx2 - xx1);
      auto h = std::max(0.0f, yy2 - yy1);
      auto inter = w * h;
      auto ovr = inter / (iarea + jarea - inter);
      if (ovr > iou_thres)
        suppressed[j] = 1;
    }
  }

  std::vector<const float *> faces;
  for (size_t i = 0; i < num_to_keep; i++)
  {
    const float *detection = candidates[keep[i]];
    faces.push_back(detection);
  }
  return faces;
}

std::vector<Detection> scale_coords(Size img1_shape, std::vector<const float *> const &coords,
                                    Size img0_shape)
{
  // Rescale coords (centre-x, centre-y, width, height) from img1_shape to img0_shape
  // calculate from img0_shape
  double gain = std::min((double)img1_shape.height / (double)img0_shape.height,
                         (double)img1_shape.width / (double)img0_shape.width); // gain = old / new;
  double padw = ((double)img1_shape.width - img0_shape.width * gain) / 2.0;
  double padh = ((double)img1_shape.height - img0_shape.height * gain) / 2.0; // wh padding

  std::vector<Detection> scaled;
  for (auto box : coords)
  {
    Detection d{{int((box[0] - box[2] / 2 - padw) / gain),
            int((box[1] - box[3] / 2 - padh) / gain),
            int(box[2] / gain),
            int(box[3] / gain)},
           std::array<Point, 5>{
             Point{int((box[5] - padw) / gain), int((box[6] - padh) / gain)},
             Point{int((box[7] - padw) / gain), int((box[8] - padh) / gain)},
             Point{int((box[9] - padw) / gain), int((box[10] - padh) / gain)},
             Point{int((box[11] - padw) / gain), int((box[12] - padh) / gain)},
             Point{int((box[13] - padw) / gain), int((box[14] - padh) / gain)}}};
    scaled.push_back(d);
  }
  return scaled;
}

void clip_coords(std::vector<Rect> &boxes, Size img_shape)
{
//     // Clip bounding xyxy bounding boxes to image shape(height, width)
//     boxes[:, 0].clamp_(0, img_shape[1]); // x1
//     boxes[:, 1].clamp_(0, img_shape[0]); // y1
//     boxes[:, 2].clamp_(0, img_shape[1]); // x2
//     boxes[:, 3].clamp_(0, img_shape[0]); // y2
for (auto& d : boxes)
{
  if(d.x < 0){
    d.width+=d.x;
    d.x=0;
  }
  if(d.y < 0){
    d.height+=d.y;
    d.y=0;
  }
  if(d.x+d.width > img_shape.width){
    d.width=img_shape.width-d.x;
  }
  if(d.y+d.height > img_shape.height){
    d.height=img_shape.height-d.y;
  }
}
}

bool filter_bb(Rect bb, Size min_size)
{
  return bb.width >= min_size.width && bb.height >= min_size.height;
}

Rect bbox_to_roi(Rect bb)
{
  bb.x /= 64;
  bb.y /= 64;
  bb.width /= 64;
  bb.width++;
  bb.height /= 64;
  bb.height++;
  return bb;
}

Size calculate_roi_size(uint32_t img_width, uint32_t img_height)
{
  int map_width = ceil(img_width / 64.0);
  int map_height = ceil(img_height / 64.0);

  return {map_width, map_height};
}

Rect enlarge_bb(Detection face)
{
  Rect ret = face.bbox;

  //enlarge bounding box from a top
  double bb_top_enlargement = 0.1;
  int addition = bb_top_enlargement * face.bbox.height;
  ret.y -= addition;
  ret.height += 2 * addition;

  //enlarge bounding box from a side using face direction
  double box_hor_centre = face.bbox.x + face.bbox.width / 2.0;
  double landm_hor_centre = 0.0;
  for (auto point : face.landmarks)
  {
    landm_hor_centre += point.x;
  }
  landm_hor_centre /= 5.0;

  double hor_offset = landm_hor_centre - box_hor_centre;
  //increase box size on the opposite size
  double bb_side_enlargement = 1.0;
  if (hor_offset < 0)
  {
    ret.width += hor_offset * bb_side_enlargement;
  }
  else
  {
    ret.x -= hor_offset * bb_side_enlargement;
    ret.width += hor_offset * bb_side_enlargement;
  }

  return ret;
}

Ort::SessionOptions get_session_options(bool cuda, int threads)
{
  using namespace std::literals;
  auto providers = Ort::GetAvailableProviders();

  Ort::SessionOptions options;
  OrtCUDAProviderOptions cuda_options;
  options.SetIntraOpNumThreads(threads);
  options.SetInterOpNumThreads(threads);
  if (cuda && std::find(providers.begin(), providers.end(), "CUDAExecutionProvider"s) == providers.end())
  {
    cuda_options.device_id = 0;
    cuda_options.arena_extend_strategy = 0;
    cuda_options.gpu_mem_limit = SIZE_MAX;
    cuda_options.cudnn_conv_algo_search = OrtCudnnConvAlgoSearchExhaustive;
    cuda_options.do_copy_in_default_stream = 1;
    options.AppendExecutionProvider_CUDA(cuda_options);
  }

  return options;
}
}

Roi RoiMapFilter::makeRoiMap(const std::vector<Rect> &bbs)
{
  Roi roi_map {width, height, std::make_unique<int8_t[]>(width*height)};

#ifdef KVAZZUP_HAVE_OPENCV
  cv::Mat new_map(height, width, CV_16S, backgroundQP-qp);
  for (auto roi : bbs)
  {
    for (int y = roi.y; y < roi.y + roi.height; y++)
      for (int x = roi.x; x < roi.x + roi.width; x++)
      {
        new_map.at<int16_t>(y,x) = (int16_t)(roiQP-qp);
      }
  }

  cv::Mat filtered;
  cv::filter2D(new_map, filtered, -1, kernel);
  cv::Mat converted;
  filtered.convertTo(converted, CV_8S);
  memcpy(roi_map.data.get(), converted.datastart, width*height);
#else
  memset(roi_map.data.get(), backgroundQP-qp, roi_map.width*roi_map.height);
  for (auto roi : bbs)
  {
    for (int y = roi.y; y < roi.y + roi.height; y++)
      for (int x = roi.x; x < roi.x + roi.width; x++)
      {
        roi_map.data[width*y+x] = (roiQP-qp);
      }
  }
#endif
  return roi_map;
}

RoiFilter::RoiFilter(QString id, StatisticsInterface *stats, std::shared_ptr<ResourceAllocator> hwResources,
                     bool cuda)
  : Filter(id, "ROI", stats, hwResources, DT_YUV420VIDEO, DT_YUV420VIDEO),
    inputSize_(-1),
    minimum_(false),
    minBbSize_{10, 10},
    drawBbox_(false),
    minRelativeBbSize_(0.5),
    useCuda_(cuda),
    roiEnabled_(false),
    frameCount_(0),
    roi_({0,0,nullptr})
{
}

RoiFilter::~RoiFilter()
{

}

void RoiFilter::updateSettings()
{
  QSettings settings(settingsFile, settingsFileFormat);

  Logger::getLogger()->printNormal(this, "Updating ROI filter settings");
  roiEnabled_ = false;
  QMutexLocker lock(&settingsMutex_);
  init();
  Filter::updateSettings();
  Logger::getLogger()->printNormal(this, "ROI filter settings updated");
}

void RoiFilter::process()
{
  std::unique_ptr<Data> input = getInput();
  assert(input->type == DT_YUV420VIDEO);
  while(input) {
    if(roiEnabled_){
      QMutexLocker lock(&settingsMutex_);
      if(inputDiscarded_ > prevInputDiscarded_) {
        skipInput_++;
        prevInputDiscarded_ = inputDiscarded_;
      }
      if(frameCount_ % skipInput_ == 0) {
        auto detections = detect(input.get());

        auto largest_bbox = find_largest_bbox(detections);
        double largest_area = largest_bbox.width * largest_bbox.height;

        std::vector<Rect> face_roi_rects;
        for (auto face : detections)
        {
          if (!filter_bb(face.bbox, minBbSize_))
          {
            continue;
          }

          double area = face.bbox.width * face.bbox.height;
          if (area / largest_area < minRelativeBbSize_)
          {
            continue;
          }

          face.bbox = enlarge_bb(face);

          if (drawBbox_)
          {
            //              cv::rectangle(rgb, face.bbox, {255, 0, 0}, 3);
            //              auto &landmarks = face.landmarks;
            //              cv::Vec3i colours[5] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 0}, {0, 255, 255}};
            //              for (size_t i = 0; i < 5; i++)
            //              {
            //                cv::circle(rgb, landmarks[i], 2, colours[i]);
            //              }
          }

          auto r = bbox_to_roi(face.bbox);
          face_roi_rects.push_back(r);
        }

        //        if(face_roi_rects.size() > 0) {
        //          Logger::getLogger()->printDebug(DebugType::DEBUG_NORMAL, this, "Found faces", {"Number"}, {QString::number(face_roi_rects.size())});
        //        }

        Size roi_size = calculate_roi_size(input->vInfo->width, input->vInfo->height);
        roi_.width = roi_size.width;
        roi_.height = roi_size.height;
        if(roi_.width != roiFilter_.width || roi_.height != roiFilter_.height) {
          roiFilter_.roi_maps.clear();
          roiFilter_.width = roi_size.width;
          roiFilter_.height = roi_size.height;
        }

        int roi_length = (roi_size.width+1)*(roi_size.height+1);

        roiFilter_.roiQP = getHWManager()->getRoiQp();
        roiFilter_.backgroundQP = getHWManager()->getBackgroundQp();

        clip_coords(face_roi_rects, roi_size);
        Roi roi_mat = roiFilter_.makeRoiMap(face_roi_rects);
        roi_.data = std::make_unique<int8_t[]>(roi_length);
        memcpy(roi_.data.get(), roi_mat.data.get(), roi_length);
      }
      if(roi_.data){
        input->vInfo->roiWidth = roi_.width;
        input->vInfo->roiHeight = roi_.height;
        input->vInfo->roiArray = std::make_unique<int8_t[]>(roi_.width*roi_.height);
        memcpy(input->vInfo->roiArray.get(), roi_.data.get(), roi_.width*roi_.height);
      }
      frameCount_++;
    }
    sendOutput(std::move(input));
    input = getInput();
  }
}

bool RoiFilter::init()
{
  bool isOk = true;
  prevInputDiscarded_ = 0;
  skipInput_ = 1;
  QSettings settings(settingsFile, settingsFileFormat);
  std::wstring newModel = settings.value(SettingsKey::roiDetectorModel).toString().toStdWString();
  kernelType_ = settings.value(SettingsKey::roiKernelType).toString().toStdString();
  kernelSize_ = settings.value(SettingsKey::roiKernelSize).toInt();
  int threads = settings.value(SettingsKey::roiMaxThreads).toInt();

#ifdef KVAZZUP_HAVE_OPENCV
  if(kernelType_ == "Gaussian"){
    roiFilter_.kernel = cv::getGaussianKernel(kernelSize_, 1.0);
  } else {
    // Default to median
    roiFilter_.kernel = cv::Mat(kernelSize_, 1, 1.0/kernelSize_);
  }
#endif

  roiFilter_.depth = settings.value(SettingsKey::roiFilterDepth).toInt();

  roiFilter_.roiQP = settings.value(SettingsKey::roiQp).toInt();
  roiFilter_.backgroundQP = settings.value(SettingsKey::backgroundQp).toInt();
  roiFilter_.qp = settings.value(SettingsKey::videoQP).toInt();

  if(newModel != model_){
    model_ = newModel;
    try {
      Ort::SessionOptions options = get_session_options(false, threads);
      session_ = std::make_unique<Ort::Session>(env_, model_.c_str(), options);
      allocator_ = std::make_unique<Ort::Allocator>(*session_, Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault));
    }  catch (std::exception& e) {
      Logger::getLogger()->printError(this, e.what());
      return isOk;
    }

    size_t input_count = session_->GetInputCount();
    if (input_count != 1)
    {
      Logger::getLogger()->printError(this, "Expected model input count to be 1");
      isOk = false;
    }

    inputName_ = session_->GetInputName(0, *allocator_);
    inputShape_ = session_->GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();

    if (inputShape_.size() != 4)
    {
      Logger::getLogger()->printError(this, "Expected model input dimensions to be 4");
      isOk = false;
    }

    if (inputShape_[0] > 1 || inputShape_[1] != 3)
    {
      Logger::getLogger()->printError(this, "Expected model with input shape [-1, 3, height, width]");
      isOk = false;
    }

    if ((inputShape_[2] == -1 || inputShape_[3] == -1) && inputShape_[2] != inputShape_[3])
    {
      Logger::getLogger()->printError(this, "Model width or height is dynamic while the other is not");
      isOk = false;
    }

    outputName_ = session_->GetOutputName(0, *allocator_);
    outputShape_ = session_->GetOutputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
    //size_t output_count = session.GetOutputCount();

    if (inputShape_[2] != -1)
    {
      Logger::getLogger()->printWarning(this,"Using model with fixed input size.\nUser provided input size ignored.\n", "Input shape", QString::number(inputShape_[3])+","+QString::number(inputShape_[2]));
      inputSize_ = std::max(inputShape_[2], inputShape_[3]);
      minimum_ = false;
    }
  }

  roiEnabled_ = isOk && settings.value(SettingsKey::roiEnabled).toBool();
  return isOk;
}

void RoiFilter::close()
{
  allocator_.reset();
  session_.reset();
}

namespace {
void copyConvert(uint8_t* src, float* dst, size_t len) {
  for(size_t i = 0; i < len; i++) {
    dst[i] = float(src[i]) / 255.0f;
  }
}
}

//#include <memory>

std::vector<Detection> RoiFilter::detect(const Data* input)
{
  Size original_size{input->vInfo->width, input->vInfo->height};
  std::vector<float> Y_f(original_size.width*original_size.height);
  copyConvert(input->data.get(), Y_f.data(), original_size.width*original_size.height);

  //cv::Mat Y(input->vInfo->height, input->vInfo->width, CV_8U, input->data.get());

  auto Y_input = letterbox(Y_f, original_size,{inputSize_, inputSize_}, 114, minimum_);
  auto channel_size = Y_input.size();
  Y_input.resize(Y_input.size()*3);
  std::memcpy(Y_input.data()+channel_size, Y_input.data(), channel_size);
  std::memcpy(Y_input.data()+channel_size*2, Y_input.data(), channel_size);

/*
  //cv::cvtColor(input, input, cv::COLOR_RGBA2RGB);
  Y_input.convertTo(Y_input, CV_32FC3, 1.0 / 255.0);

  // Copy Y to r, g and b channels fow bw rgb image avoiding color conversions
  int size[] = {3, Y_input.size[0], Y_input.size[1]};
  cv::Mat channels(3, size, CV_32F);

  cv::Mat out[3] = {cv::Mat(Y_input.size[0], Y_input.size[1], CV_32F, channels.data),
                    cv::Mat(Y_input.size[0], Y_input.size[1], CV_32F, channels.data + channels.step[0]),
                    cv::Mat(Y_input.size[0], Y_input.size[1], CV_32F, channels.data + channels.step[0] * 2)};
  Y_input.copyTo(out[0]);
  Y_input.copyTo(out[1]);
  Y_input.copyTo(out[2]);
*/

  int64_t shape[4] = {1, 3, inputSize_, inputSize_};
  auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
  auto input_tensor = Ort::Value::CreateTensor<float>(&*memory, Y_input.data(), Y_input.size(), shape, 4);

  Ort::RunOptions options;
  auto detections = session_->Run(options, &inputName_, &input_tensor, 1, &outputName_, 1);
  auto out_shape = detections[0].GetTensorTypeAndShapeInfo().GetShape();
  auto faces = non_max_suppression_face(detections[0]);

  auto scaled_detections = scale_coords({inputSize_, inputSize_}, faces, original_size);

  return scaled_detections;
}
