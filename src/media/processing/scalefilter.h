#pragma once

#include "filter.h"

#include <QSize>

// A filter that can scale video frame.

class ScaleFilter : public Filter
{
public:
  ScaleFilter(QString id, StatisticsInterface *stats,
              std::shared_ptr<ResourceAllocator> hwResources);

  void setResolution(QSize newResolution);

  void process();

  std::unique_ptr<Data> scaleFrame(std::unique_ptr<Data> input);

private:

  QSize newSize_;
};
