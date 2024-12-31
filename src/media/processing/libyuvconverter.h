#pragma once

#include "filter.h"

#include <QSize>

class LibYUVConverter : public Filter
{
public:
  LibYUVConverter(QString id, StatisticsInterface* stats,
                  std::shared_ptr<ResourceAllocator> hwResources, DataType input);

  void setConferenceSize(uint32_t otherParticipants);

  virtual void updateSettings();

protected:

  void process();

private:

  uint32_t getFourCC(DataType type) const;

  QSize resolution_;

  uint32_t participants_;

  QMutex resolutionMutex_;
};
