#include "roiyolofilter.h"

#include "ui/gui/videointerface.h"

#include "logger.h"
#include "settingskeys.h"
#include "media/resourceallocator.h"
#include "global.h"
#include "common.h"

#ifdef KVAZZUP_HAVE_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#endif

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

#include <algorithm>


ROIYoloFilter::ROIYoloFilter(QString id, StatisticsInterface *stats, std::shared_ptr<ResourceAllocator> hwResources,
                     bool cuda, VideoInterface* roiInterface)
  : Filter(id, "RoI", stats, hwResources, DT_YUV420VIDEO, DT_YUV420VIDEO),
    prevInputDiscarded_(0),
    skipInput_(1),
    minBbSize_{0, 0},
    drawBbox_(false),
    minRelativeBbSize_(0),
    useCuda_(cuda),
    roiEnabled_(false),
    frameCount_(0),
    roi_({0,0,nullptr}),
    roiSurface_(roiInterface),
    faceDetection_(false)
{}


ROIYoloFilter::~ROIYoloFilter()
{}


void ROIYoloFilter::updateSettings()
{
  Logger::getLogger()->printNormal(this, "Updating RoI filter settings");
  QMutexLocker lock(&settingsMutex_);
  prevInputDiscarded_ = 0;
  skipInput_ = 1;

  init();
  Filter::updateSettings();
}


bool ROIYoloFilter::init()
{
  QSettings settings(settingsFile, settingsFileFormat);

  roiSettings_.roiQP = settings.value(SettingsKey::roiQp).toInt();
  roiSettings_.backgroundQP = settings.value(SettingsKey::backgroundQp).toInt();
  roiSettings_.qp = settings.value(SettingsKey::videoQP).toInt();
  QString newModelQstr = settings.value(SettingsKey::roiDetectorModel).toString();

  roiEnabled_ = settingValue(SettingsKey::roiEnabled) && !newModelQstr.isEmpty();
  int threads = settings.value(SettingsKey::roiMaxThreads).toInt();
  if (roiEnabled_)
  {
    roiEnabled_ = initYolo(threads, newModelQstr);
    return roiEnabled_;
  }

  return true;
}


void ROIYoloFilter::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    if(roiEnabled_ && getHWManager()->useAutoROI())
    {
      QMutexLocker lock(&settingsMutex_);

      if(inputDiscarded_ > prevInputDiscarded_)
      {
        skipInput_++;
        prevInputDiscarded_ = inputDiscarded_;
      }

      if(frameCount_ % skipInput_ == 0)
      {
        // run onnx yolo face detection
        std::vector<Detection> detections = yolo_detection(input.get());

        // find detection with largest bounding box
        Rect largest_bbox = find_largest_bbox(detections);
        double largest_area_pix = largest_bbox.width * largest_bbox.height;

        std::vector<Rect> face_roi_rects;
        for (Detection& face : detections)
        {
          double area = face.bbox.width * face.bbox.height;

          // filter out small faces
          if (!filter_bb(face.bbox, minBbSize_) || area / largest_area_pix < minRelativeBbSize_)
          {
            continue;
          }

          face.bbox = enlarge_bb(face);
          Rect roi = bbox_to_roi(face.bbox);
          face_roi_rects.push_back(roi);

#ifdef KVAZZUP_HAVE_OPENCV
          if (drawBbox_)
          {
            cv::rectangle(rgb, face.bbox, {255, 0, 0}, 3);
            auto &landmarks = face.landmarks;
            cv::Vec3i colours[5] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 0}, {0, 255, 255}};
            for (size_t i = 0; i < 5; i++)
            {
              cv::circle(rgb, landmarks[i], 2, colours[i]);
            }
          }
#endif
        }

        Size roi_size = calculate_roi_size(input->vInfo->width, input->vInfo->height);
        roi_.width = roi_size.width;
        roi_.height = roi_size.height;

        if(roi_.width != roiSettings_.width || roi_.height != roiSettings_.height)
        {
          roiSettings_.width = roi_size.width;
          roiSettings_.height = roi_size.height;
        }

        int roi_length = (roi_size.width+1)*(roi_size.height+1);

        roiSettings_.roiQP = getHWManager()->getRoiQp();
        roiSettings_.backgroundQP = getHWManager()->getBackgroundQp();

        clip_coords(face_roi_rects, roi_size);
        RoiMap roi_mat = makeRoiMap(face_roi_rects);
        roi_.data = std::make_unique<int8_t[]>(roi_length);
        memcpy(roi_.data.get(), roi_mat.data.get(), roi_length);

        roiSurface_->inputDetections(detections, {input->vInfo->width, input->vInfo->height}, 0);
      }

      if(roi_.data)
      {
        input->vInfo->roi.width = roi_.width;
        input->vInfo->roi.height = roi_.height;
        input->vInfo->roi.data = std::make_unique<int8_t[]>(roi_.width*roi_.height);
        memcpy(input->vInfo->roi.data.get(), roi_.data.get(), roi_.width*roi_.height);

        roiSurface_->visualizeROIMap(input->vInfo->roi, roiSettings_.qp);
      }

      frameCount_++;
    }

    sendOutput(std::move(input));
    input = getInput();
  }
}


bool ROIYoloFilter::initYolo(int threads, QString newModelQstr)
{
#ifdef _WIN32
  std::wstring newModelPath = newModelQstr.toStdWString();
#else
  std::string newModelPath = newModelQstr.toStdString();
#endif

#ifdef KVAZZUP_HAVE_OPENCV
    kernelSize_ = settings.value(SettingsKey::roiKernelSize).toInt();
    kernelType_ = settings.value(SettingsKey::roiKernelType).toString().toStdString();
    if (kernelType_ == "Gaussian") {
      roiSettings_.kernel = cv::getGaussianKernel(kernelSize_, 1.0);
    } else {
      // Default to median
      roiSettings_.kernel = cv::Mat(kernelSize_, 1, 1.0 / kernelSize_);
    }
#endif

  if (newModelPath != yoloModelPath_)
  {
    Logger::getLogger()->printNormal(this, "Initializing Yolo model");

      yoloModelPath_ = newModelPath;
    try
    {
      if (yoloModel_.session)
      {
        // Deallocate previous strings before destroying their allocator.
        yoloModel_.inputName = std::nullopt;
        yoloModel_.outputName = std::nullopt;
      }

      Ort::SessionOptions options = get_session_options(false, threads);
      yoloModel_.session = std::make_unique<Ort::Session>(onnxEnv_, yoloModelPath_.c_str(), options);
      yoloModel_.allocator = std::make_unique<Ort::Allocator>(*yoloModel_.session,
                                                        Ort::MemoryInfo::CreateCpu(OrtArenaAllocator,
                                                                                   OrtMemTypeDefault));
    }
    catch (std::exception &e)
    {
      Logger::getLogger()->printError(this, e.what());
      return false;
    }

    size_t input_count = yoloModel_.session->GetInputCount();
    if (input_count != 1)
    {
      Logger::getLogger()->printError(this, "Expected model input count to be 1");
      return false;
    }

    yoloModel_.inputName = yoloModel_.session->GetInputNameAllocated(0, *yoloModel_.allocator);
    yoloModel_.inputShape = yoloModel_.session->GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();

    if (yoloModel_.inputShape.size() != 4 ||
        yoloModel_.inputShape[0] > 1 ||
        yoloModel_.inputShape[1] != 3 ||
        (yoloModel_.inputShape[2] == -1 || yoloModel_.inputShape[3] == -1) && yoloModel_.inputShape[2] != yoloModel_.inputShape[3])
    {
      Logger::getLogger()->printError(this, "Expected model input dimensions to be 4");
      Logger::getLogger()->printError(this, "Expected model with input shape [-1, 3, height, width]");
      Logger::getLogger()->printError(this, "Model width or height is dynamic while the other is not");
      return false;
    }

    yoloModel_.outputName = yoloModel_.session->GetOutputNameAllocated(0, *yoloModel_.allocator);
    yoloModel_.outputShape = yoloModel_.session->GetOutputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();

    if (yoloModel_.outputShape[2] == 16)
    {
      //yolov5-face
      faceDetection_ = true;
    }
    else if (yoloModel_.outputShape[2] == 85)
    {
      //yolov5 object
      faceDetection_ = false;
    }

    if (yoloModel_.inputShape[2] != -1)
    {
      Logger::getLogger()->printWarning(this, "Using model with fixed input size.\n"
                                              "User provided input size ignored.\n",
                                        "Input shape", QString::number(yoloModel_.inputShape[3]) + "," + QString::number(yoloModel_.inputShape[2]));

      yoloModel_.inputSize = std::max(yoloModel_.inputShape[2], yoloModel_.inputShape[3]);
      yoloModel_.minimum = false;
    }

    Logger::getLogger()->printNormal(this, "Yolo model initialized");
  }

  return true;
}


void ROIYoloFilter::close()
{
  yoloModel_.allocator.reset();
  yoloModel_.session.reset();
}


void ROIYoloFilter::YUVToFloat(uint8_t* src, float* dst, size_t len)
{
  for(size_t i = 0; i < len; i++)
  {
    dst[i] = float(src[i]) / 255.0f; // divide color space to 0-1
  }
}


std::vector<Detection> ROIYoloFilter::yolo_detection(const Data* input)
{
  Size original_size{input->vInfo->width, input->vInfo->height};
  std::vector<float> Y_f(original_size.width*original_size.height);
  YUVToFloat(input->data.get(), Y_f.data(), original_size.width*original_size.height);

  const uint8_t COLOR = 114;
  auto Y_input = scaleToLetterbox(Y_f, original_size,{yoloModel_.inputSize, yoloModel_.inputSize}, COLOR, yoloModel_.minimum);
  size_t channel_size = Y_input.size();

  Y_input.resize(Y_input.size()*3);
  std::memcpy(Y_input.data() + channel_size,   Y_input.data(), channel_size);
  std::memcpy(Y_input.data() + 2*channel_size, Y_input.data(), channel_size);

  const size_t SHAPE_LEN = 4;
  int64_t shape[SHAPE_LEN] = {1, 3, yoloModel_.inputSize, yoloModel_.inputSize};
  auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

  auto input_tensor = Ort::Value::CreateTensor<float>(&*memory, Y_input.data(), Y_input.size(), shape, SHAPE_LEN);

  Ort::RunOptions options;
  const char* input_names[] = {yoloModel_.inputName->get()};
  char* output_names[]      = {yoloModel_.outputName->get()};

  // run the detection model
  auto detections = yoloModel_.session->Run(options, input_names, &input_tensor, 1, output_names, 1);

  auto out_shape = detections[0].GetTensorTypeAndShapeInfo().GetShape();

  // remove overlapping detections
  std::vector<const float*> objects;
  objects = non_max_suppression_obj(detections[0], faceDetection_);

  // return scaled detections
  return scale_coords({yoloModel_.inputSize, yoloModel_.inputSize}, objects, original_size);
}


Rect ROIYoloFilter::find_largest_bbox(std::vector<Detection> &detections)
{
  Rect largest = {0, 0, 0, 0};
  int largest_area = 0;

  for (auto &d : detections)
  {
    int areaPix = d.bbox.height * d.bbox.width;
    if (areaPix > largest_area)
    {
      largest_area = areaPix;
      largest = d.bbox;
    }
  }
  return largest;
}


std::vector<float> ROIYoloFilter::scaleToLetterbox(const std::vector<float>& img, Size original_shape, Size new_shape, uint8_t color,
                             bool minimum, bool scaleFill, bool scaleup)
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
  int left = int(std::round(ddw - 0.1));

#ifdef KVAZZUP_HAVE_OPENCV
  int bottom = int(std::round(ddh + 0.1));
  int right = int(std::round(ddw + 0.1));
  cv::Mat scaled;
#else
  std::vector<float> scaled_img((dw + new_unpad.width)*(dh + new_unpad.height), color/255.0f);
#endif

  if (original_shape.width != new_unpad.width && original_shape.height != new_unpad.height) // resize
  {
#ifdef KVAZZUP_HAVE_OPENCV
    cv::Mat input(original_shape.height, original_shape.width, CV_32F, (void*)img.data());
    cv::resize(input, scaled, {new_unpad.width, new_unpad.height}, 0.0, 0.0, cv::INTER_NEAREST);
#else
    double fx = 1/((double)new_unpad.width / (double)original_shape.width);
    double fy = 1/((double)new_unpad.height / (double)original_shape.height);
    for(int y = top; y < new_unpad.height+top; y++) {
      for(int x = left; x < new_unpad.width+left; x++) {
        int old_x = std::floor(x*fx),
            old_y = std::floor((y - top)*fy);
        scaled_img[y*(new_unpad.width + dw)+x] = img[old_y*original_shape.width + old_x];
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


std::vector<const float*> ROIYoloFilter::non_max_suppression_obj(
        Ort::Value const &prediction, bool faceDetection,
        double conf_thres, double iou_thres)
{
    // Non-Maximum Suppression (NMS) on inference results to reject overlapping detections

    // list of detections, on (n,6) tensor per image [xyxy, conf, cls]

    assert( 0 <= conf_thres <= 1 && "Invalid Confidence threshold {conf_thres}, valid values are between 0.0 and 1.0");
    assert( 0 <= iou_thres <= 1 && "Invalid IoU {iou_thres}, valid values are between 0.0 and 1.0");

    assert(prediction.GetTensorTypeAndShapeInfo().GetElementType() == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT);
    assert(!prediction.IsSparseTensor());
    assert(prediction.IsTensor());

    auto shape = prediction.GetTensorTypeAndShapeInfo().GetShape();

    const float *tensorData = prediction.GetTensorData<float>();
    int64_t row_size = shape[2];

    std::vector<const float *> candidates;
    if (faceDetection)
    {
      for (int64_t i = 0; i < shape[1]; i++)
      {
        const float *row = tensorData + i * row_size;
        if (row[4] > conf_thres)
        {
          candidates.push_back(row);
        }
      }
    }
    else
    {
      size_t target_object = getHWManager()->getRoiObject();

      for (int64_t i = 0; i < shape[1]; i++)
      {
        const float *row = tensorData + i * row_size;
        if (row[4] < conf_thres)
        {
          continue;
        }
        if (row[target_object+5] > conf_thres)
        {
          candidates.push_back(row);
        }
      }
    }

    std::vector<size_t> order(candidates.size());
    std::iota(order.begin(), order.end(), 0); // 0, 1, 2, 3, ...
    std::sort(order.begin(), order.end(),
              [&candidates](size_t i1, size_t i2)
              {
                  return candidates[i1] < candidates[i2];
              });
    std::vector<uint8_t> suppressed(candidates.size(), 0);
    std::vector<size_t> keep(candidates.size(), 0);
    size_t num_to_keep = 0;

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

    std::vector<const float *> objects;
    for (size_t i = 0; i < num_to_keep; i++)
    {
      const float *detection = candidates[keep[i]];
      objects.push_back(detection);
    }
    return objects;
}


std::vector<Detection> ROIYoloFilter::scale_coords(Size img1_shape, std::vector<const float *> const &coords,
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


void ROIYoloFilter::clip_coords(std::vector<Rect> &boxes, Size img_shape)
{
  //     // Clip bounding xyxy bounding boxes to image shape(height, width)
  //     boxes[:, 0].clamp_(0, img_shape[1]); // x1
  //     boxes[:, 1].clamp_(0, img_shape[0]); // y1
  //     boxes[:, 2].clamp_(0, img_shape[1]); // x2
  //     boxes[:, 3].clamp_(0, img_shape[0]); // y2
  for (auto& d : boxes)
  {
    if (d.x < 0)
    {
      d.width+=d.x;
      d.x=0;
    }
    if (d.y < 0)
    {
      d.height+=d.y;
      d.y=0;
    }
    if (d.x+d.width > img_shape.width)
    {
      d.width=img_shape.width-d.x;
    }
    if (d.y+d.height > img_shape.height)
    {
      d.height=img_shape.height-d.y;
    }
  }
}


bool ROIYoloFilter::filter_bb(Rect bb, Size min_size)
{
  return bb.width >= min_size.width && bb.height >= min_size.height;
}


Rect ROIYoloFilter::bbox_to_roi(Rect bb)
{
  int x2 = (bb.x+bb.width - 64/10) / 64;
  int y2 = (bb.y+bb.height - 64/10) / 64;
  bb.x = (bb.x + 64/10)/64;
  bb.y = (bb.y + 64/10)/64;
  bb.width = x2-bb.x;
  bb.width++;
  bb.height = y2-bb.y;
  bb.height++;
  if (bb.width < 1)
  {
    bb.width = 1;
  }
  if (bb.height < 1)
  {
    bb.height = 1;
  }
  return bb;
}


Size ROIYoloFilter::calculate_roi_size(uint32_t img_width, uint32_t img_height)
{
  int map_width = ceil(img_width / 64.0);
  int map_height = ceil(img_height / 64.0);

  return {map_width, map_height};
}


Rect ROIYoloFilter::enlarge_bb(Detection face)
{
  Rect ret = face.bbox;

  //enlarge bounding box from a top
  double bb_top_enlargement = 0.1;
  int addition = bb_top_enlargement * face.bbox.height;
  ret.y -= addition;
  ret.height += 2 * addition;

  return ret;
}


Ort::SessionOptions ROIYoloFilter::get_session_options(bool cuda, int threads)
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


RoiMap ROIYoloFilter::makeRoiMap(const std::vector<Rect> &bbs)
{
  int width = roiSettings_.width;
  int height = roiSettings_.width;
  int roiQP = roiSettings_.roiQP;
  int backgroundQP = roiSettings_.backgroundQP;
  int qp = roiSettings_.qp;

  RoiMap roi_map {width, height, std::make_unique<int8_t[]>(width*height)};

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
  memset(roi_map.data.get(), backgroundQP - qp, roi_map.width*roi_map.height);
  for (auto roi : bbs)
  {
    for (int y = roi.y; y < roi.y + roi.height; y++)
      for (int x = roi.x; x < roi.x + roi.width; x++)
      {
        roi_map.data[width*y+x] = (roiQP - qp);
      }
  }
#endif
  return roi_map;
}
