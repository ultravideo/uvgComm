#pragma once
#include "filter.h"

#include <QAudioFormat>

class AECProcessor;

class AECInputFilter : public Filter
{
public:
  AECInputFilter(QString id, StatisticsInterface* stats);
  ~AECInputFilter();

  void initInput(QAudioFormat format);

  std::shared_ptr<AECProcessor> getAEC()
  {
    return aec_;
  }

protected:

  void process();

private:

  std::shared_ptr<AECProcessor> aec_;
};
