#pragma once

#include "filter.h"

#include <memory>

class AECProcessor;

class AECPlaybackFilter : public Filter
{
public:
  AECPlaybackFilter(QString id, StatisticsInterface *stats,
                    std::shared_ptr<AECProcessor> processor);

protected:

  void process();

private:

  std::shared_ptr<AECProcessor> aec_;
};
