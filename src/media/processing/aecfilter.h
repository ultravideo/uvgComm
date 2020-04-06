#pragma once
#include "filter.h"

#include <QAudioFormat>

class AECProcessor;

enum AECType {AEC_INPUT, AEC_ECHO};

class AECFilter : public Filter
{
public:
  AECFilter(QString id, StatisticsInterface* stats, QAudioFormat format,
            AECType type, std::shared_ptr<AECProcessor> echo_state = nullptr);
  ~AECFilter();

  std::shared_ptr<AECProcessor> getAEC()
  {
    return aec_;
  }

protected:

  void process();

private:

  AECType type_;

  std::shared_ptr<AECProcessor> aec_;
};
