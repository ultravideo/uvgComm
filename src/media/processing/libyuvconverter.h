#pragma once

#include "filter.h"

#include <QSize>

class LibYUVConverter : public Filter
{
public:
  LibYUVConverter(QString id, StatisticsInterface* stats,
                  std::shared_ptr<ResourceAllocator> hwResources, DataType input);

  void changeResolution();

  // Force target resolution (used to keep libyuv in sync with encoder)
  void setTargetResolution(const QSize& resolution);

  virtual void updateSettings();

protected:

  void process();

private:

  uint32_t getFourCC(DataType type) const;

  QSize targetResolution_;
  QSize baseResolution_;

  // If true, targetResolution_ is set externally and changeResolution() should not
  // override it from HW manager.
  bool manualTargetResolution_ = false;

  QMutex resolutionMutex_;
};
