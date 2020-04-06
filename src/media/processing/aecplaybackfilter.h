#pragma once

#include "filter.h"

#include <memory>

class AECProcessor;

class AECPlaybackFilter : public Filter
{
public:
  AECPlaybackFilter(QString id, StatisticsInterface *stats, uint32_t sessionID,
                    std::shared_ptr<AECProcessor> processor);

protected:

  void process();

private:

  uint32_t sessionID_;

  std::shared_ptr<AECProcessor> aec_;
};
