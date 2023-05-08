#pragma once

#include "filter.h"
#include "audiooutputdevice.h"

#include <QAudioFormat>

// There should be only one audio output filter

class AudioOutputFilter : public Filter
{
  Q_OBJECT
public:
  AudioOutputFilter(QString id, StatisticsInterface* stats,
                    std::shared_ptr<ResourceAllocator> hwResources,
                    QAudioFormat format);

  virtual void updateSettings();

signals:
  void outputtingSound();

protected:
  void process();

private:

  AudioOutputDevice output_;
};
