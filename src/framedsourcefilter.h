#pragma once
#include "filter.h"

#include <FramedSource.hh>
#include <QMutex>
#include <QSemaphore>

class StatisticsInterface;

class FramedSourceFilter : public FramedSource, public Filter
{
public:
  FramedSourceFilter(QString id, StatisticsInterface* stats,
                     UsageEnvironment &env, DataType type, QString media);



  // called by live555. Takes a sample from input and schedules it to be sent.
  virtual void doGetNextFrame();

  virtual void updateSettings();

  virtual Boolean isH265VideoStreamFramer() const
  {
    return type_ == HEVCVIDEO;
  }

  void sendComplete();
protected:
  void process();

private:

  void copyFrameToBuffer(std::unique_ptr<Data> currentFrame);

  DataType type_; // TODO use output_

  QSemaphore framePointerReady_;

  EventTriggerId afterEvent_;

  bool separateInput_;
};
