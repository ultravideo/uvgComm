#pragma once

#include "filter.h"

#include <QSize>

class LibYUVConverter : public Filter
{
public:
  LibYUVConverter(QString id, StatisticsInterface* stats,
                  std::shared_ptr<ResourceAllocator> hwResources, DataType input);

  void changeResolution();

  virtual void updateSettings();

protected:

  void process();

private:

  uint32_t getFourCC(DataType type) const;

  QSize targetResolution_;
  QSize baseResolution_;

  QMutex resolutionMutex_;
};
