#include "roifilter.h"
#include "logger.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include "logger.h"
#include "common.h"
#include "settingskeys.h"
#include "yuvconversions.h"

#include <opencv2/imgproc.hpp>

#ifdef max
#undef max
#endif

namespace
{
cv::Rect find_largest_bbox(std::vector<Detection> &detections)
{
  cv::Rect largest;
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

cv::Mat letterbox(cv::Mat img, cv::Size new_shape, cv::Scalar color = {114, 114, 114},
                  bool minimum = false, bool scaleFill = false, bool scaleup = true)
{
  //Resize image to a 32 - pixel - multiple rectangle https://github.com/ultralytics/yolov3/issues/232
  cv::Size shape(img.size[1], img.size[0]); // current shape [width, height]

  // Scale ratio(new / old)
  double r = std::min((double)new_shape.width / (double)shape.width,
                      (double)new_shape.height / (double)shape.height);
  if (!scaleup) // only scale down, do not scale up (for better test mAP)
  {
    r = std::min(r, 1.0);
  }
  // Compute padding
  cv::Size new_unpad(int(std::round(shape.width * r)), int(std::round(shape.height * r)));
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

  if (shape != new_unpad) // resize
  {
    cv::resize(img, img, new_unpad, 0.0, 0.0, cv::INTER_NEAREST);
  }

  double ddh = (double)dh / 2.0; // divide padding into 2 sides
  double ddw = (double)dw / 2.0;
  int top = int(std::round(ddh - 0.1));
  int bottom = int(std::round(ddh + 0.1));
  int left = int(std::round(ddw - 0.1));
  int right = int(std::round(ddw + 0.1));

  cv::copyMakeBorder(img, img, top, bottom, left, right, cv::BORDER_CONSTANT, color); // add border
  return img;
}

std::vector<const float *> non_max_suppression_face(Ort::Value const &prediction, double conf_thres = 0.25, double iou_thres = 0.45)
{
  assert(prediction.GetTensorTypeAndShapeInfo().GetElementType() == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT);
  assert(!prediction.IsSparseTensor());
  assert(prediction.IsTensor());

  auto shape = prediction.GetTensorTypeAndShapeInfo().GetShape();
  assert(shape.size() == 3 && shape[0] == 1 && shape[2] == 16);

  int nc = prediction.GetTensorTypeAndShapeInfo().GetShape()[2] - 15; // number of classes
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
  std::vector<cv::Rect> output;
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

std::vector<Detection> scale_coords(cv::Size img1_shape, std::vector<const float *> const &coords,
                                    cv::Size img0_shape)
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
    scaled.push_back(
          {{int((box[0] - box[2] / 2 - padw) / gain),
            int((box[1] - box[3] / 2 - padh) / gain),
            int(box[2] / gain),
            int(box[3] / gain)},
           std::array<cv::Point, 5>{
             cv::Point{int((box[5] - padw) / gain), int((box[6] - padh) / gain)},
             cv::Point{int((box[7] - padw) / gain), int((box[8] - padh) / gain)},
             cv::Point{int((box[9] - padw) / gain), int((box[10] - padh) / gain)},
             cv::Point{int((box[11] - padw) / gain), int((box[12] - padh) / gain)},
             cv::Point{int((box[13] - padw) / gain), int((box[14] - padh) / gain)}}});
  }
  return scaled;
}

// std::vector<cv::Rect> clip_coords(std::vector<cv::Rect2f> const &boxes, cv::Rect img_shape)
// {
//     // Clip bounding xyxy bounding boxes to image shape(height, width)
//     boxes[:, 0].clamp_(0, img_shape[1]); // x1
//     boxes[:, 1].clamp_(0, img_shape[0]); // y1
//     boxes[:, 2].clamp_(0, img_shape[1]); // x2
//     boxes[:, 3].clamp_(0, img_shape[0]); // y2
// }

bool filter_bb(cv::Rect bb, cv::Size min_size)
{
  return bb.width >= min_size.width && bb.height >= min_size.height;
}

cv::Rect bbox_to_roi(cv::Rect bb)
{
  bb.x /= 64;
  bb.y /= 64;
  bb.width /= 64;
  bb.width++;
  bb.height /= 64;
  bb.height++;
  return bb;
}

cv::Point calculate_roi_size(uint32_t img_width, uint32_t img_height)
{
  int map_width = ceil(img_width / 64.0);
  int map_height = ceil(img_height / 64.0);

  return {map_width, map_height};
}

cv::Rect enlarge_bb(Detection face)
{
  cv::Rect ret = face.bbox;

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

Ort::SessionOptions get_session_options(bool cuda)
{
  using namespace std::literals;
  auto providers = Ort::GetAvailableProviders();

  if (!cuda || std::find(providers.begin(), providers.end(), "CUDAExecutionProvider"s) == providers.end())
  {
    return Ort::SessionOptions();
  }
  Ort::SessionOptions options;
  OrtCUDAProviderOptions cuda_options;
  cuda_options.device_id = 0;
  cuda_options.arena_extend_strategy = 0;
  cuda_options.gpu_mem_limit = SIZE_MAX;
  cuda_options.cudnn_conv_algo_search = OrtCudnnConvAlgoSearchExhaustive;
  cuda_options.do_copy_in_default_stream = 1;

  options.AppendExecutionProvider_CUDA(cuda_options);

  return options;
}
}

cv::Mat RoiMapFilter::makeRoiMap(const std::vector<cv::Rect> &bbs)
{
  cv::Mat new_map(height, width, CV_8U, backgroundQP - INT8_MIN);

  for (auto roi : bbs)
  {
    for (int y = roi.y; y < roi.y + roi.height; y++)
      for (int x = roi.x; x < roi.x + roi.width; x++)
      {
        new_map.data[y * new_map.step[0] + x] = (uint8_t)(dQP - INT8_MIN);
      }
  }

  cv::Mat filtered;
  cv::filter2D(new_map, filtered, -1, kernel);
  new_map.convertTo(filtered, CV_8S, 1, INT8_MIN);
  return filtered;
}

RoiFilter::RoiFilter(QString id, QString name, StatisticsInterface *stats, std::shared_ptr<HWResourceManager> hwResources,
                     std::wstring model, int size, bool cuda)
  : Filter(id, "ROI", stats, hwResources, DT_YUV420VIDEO, DT_YUV420VIDEO),
    input_size(size ? size : -1),
    minimum(false),
    min_bb_size(10, 10),
    draw_bbox(false),
    min_relative_bb_size(0.5),
    use_cuda(cuda),
    is_ok(false),
    frame_count(0),
    roi({0,0,nullptr})
{
}

RoiFilter::~RoiFilter()
{

}

void RoiFilter::updateSettings()
{
  Logger::getLogger()->printNormal(this, "Updating ROI filter settings");

  stop();

  while(isRunning())
  {
    sleep(1);
  }

  close();
  init();

  Filter::updateSettings();
}

void RoiFilter::process()
{
  std::unique_ptr<Data> input = getInput();
  assert(input->type == DT_YUV420VIDEO);

  if(!is_ok){
    while(input){
      sendOutput(std::move(input));
      input=getInput();
    }
  } else {
    while(input)
    {
      if(frame_count % 2) {
        auto detections = detect(input.get());

        auto largest_bbox = find_largest_bbox(detections);
        double largest_area = largest_bbox.width * largest_bbox.height;

        std::vector<cv::Rect> face_roi_rects;
        for (auto face : detections)
        {
          if (!filter_bb(face.bbox, min_bb_size))
          {
            continue;
          }

          double area = face.bbox.width * face.bbox.height;
          if (area / largest_area < min_relative_bb_size)
          {
            continue;
          }

          face.bbox = enlarge_bb(face);

          if (draw_bbox)
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

        cv::Size roi_size = calculate_roi_size(input->vInfo->width, input->vInfo->height);
        if(roi_size.width != filter.width || roi_size.height != filter.height) {
          filter.roi_maps.clear();
          filter.width = roi_size.width;
          filter.height = roi_size.height;
        }

        int roi_length = (roi_size.width+1)*(roi_size.height+1);

        cv::Mat roi_mat = filter.makeRoiMap(face_roi_rects);
        roi.data = std::make_unique<int8_t[]>(roi_length);
        memcpy(roi.data.get(), roi_mat.data, roi_length);
      }
      if(roi.data){
        input->roi.width = roi.width;
        input->roi.height = roi.height;
        input->roi.roi_array = (int8_t*)malloc(roi.width*roi.height);
        memcpy(input->roi.roi_array, roi.data.get(), roi.width*roi.height);
      }
      sendOutput(std::move(input));
      frame_count++;

      input = getInput();
    }
  }
}

bool RoiFilter::init()
{
  QSettings settings(settingsFile, settingsFileFormat);
  model = settings.value(SettingsKey::roiDetectorModel).toString().toStdWString();
  kernel_type = settings.value(SettingsKey::roiKernelType).toString().toStdString();
  kernel_size = settings.value(SettingsKey::roiKernelSize).toInt();

  if(kernel_type == "Gaussian"){
    filter.kernel = cv::getGaussianKernel(kernel_size, 1.0);
  } else {
    // Default to median
    filter.kernel = cv::Mat(kernel_size, 1, 1.0/kernel_size);
  }

  filter.depth = settings.value(SettingsKey::roiFilterDepth).toInt();

  is_ok = true;
  try {
    session = std::make_unique<Ort::Session>(env, model.c_str(), get_session_options(true));
    allocator = std::make_unique<Ort::Allocator>(*session, Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault));
  }  catch (std::exception& e) {
    Logger::getLogger()->printError(this, e.what());
    is_ok = false;
    return is_ok;
  }

  size_t input_count = session->GetInputCount();
  if (input_count != 1)
  {
    Logger::getLogger()->printError(this, "Expected model input count to be 1");
    is_ok = false;
  }

  input_name = session->GetInputName(0, *allocator);
  input_shape = session->GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();

  if (input_shape.size() != 4)
  {
    Logger::getLogger()->printError(this, "Expected model input dimensions to be 4");
    is_ok = false;
  }

  if (input_shape[0] > 1 || input_shape[1] != 3)
  {
    Logger::getLogger()->printError(this, "Expected model with input shape [-1, 3, height, width]");
    is_ok = false;
  }

  if ((input_shape[2] == -1 || input_shape[3] == -1) && input_shape[2] != input_shape[3])
  {
    Logger::getLogger()->printError(this, "Model width or height is dynamic while the other is not");
    is_ok = false;
  }

  output_name = session->GetOutputName(0, *allocator);
  output_shape = session->GetOutputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
  //size_t output_count = session.GetOutputCount();

  if (input_shape[2] != -1)
  {
    printf("Using model with fixed input size %lldx%lld.\nUser provided input size ignored.\n", input_shape[3], input_shape[2]);
    input_size = std::max(input_shape[2], input_shape[3]);
    minimum = false;
  }

  return is_ok;
}

void RoiFilter::close()
{
  allocator.reset();
  session.reset();
}

std::vector<Detection> RoiFilter::detect(const Data* input)
{
  cv::Size original_size(input->vInfo->width, input->vInfo->height);
  cv::Mat Y(input->vInfo->height, input->vInfo->width, CV_8U, input->data.get());

  cv::Mat Y_input = letterbox(Y, {input_size, input_size}, {114, 114, 114}, minimum);

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

  auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
  int64_t shape[4] = {1, channels.size[0], channels.size[1], channels.size[2]};
  auto input_tensor = Ort::Value::CreateTensor<float>(&*memory, (float *)channels.data, channels.total() * channels.elemSize1(), shape, 4);

  Ort::RunOptions options;
  auto detections = session->Run(options, &input_name, &input_tensor, 1, &output_name, 1);
  auto out_shape = detections[0].GetTensorTypeAndShapeInfo().GetShape();
  auto faces = non_max_suppression_face(detections[0]);

  auto scaled_detections = scale_coords(Y_input.size(), faces, original_size);

  return scaled_detections;
}
