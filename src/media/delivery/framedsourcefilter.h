#pragma once
#include "filter.h"

#include <FramedSource.hh>
#include <QMutex>
#include <QSemaphore>


// This is the first step in sending data using RTP. Rest of the steps are inside live555.

class StatisticsInterface;

class FramedSourceFilter : public FramedSource, public Filter
{
public:
  FramedSourceFilter(QString id, StatisticsInterface* stats,
                     UsageEnvironment &env, DataType type,
                     QString media, QMutex* triggerMutex, bool live555Copying);

  ~FramedSourceFilter();

  // called by live555. Takes a sample from input and schedules it to be sent.
  virtual void doGetNextFrame();

  virtual void updateSettings();

  virtual Boolean isH265VideoStreamFramer() const
  {
    return type_ == HEVCVIDEO;
  }

  virtual void start();
  virtual void stop();
protected:
  void process();

  // FramedSource function.
  virtual void doStopGettingFrames();

private:

  // copy the frame to FramedSource buffer
  void copyFrameToBuffer(std::unique_ptr<Data> currentFrame);

  // send frame over the network.
  void sendFrame();


  DataType type_; // TODO use output_

  QSemaphore framePointerReady_;

  EventTriggerId afterEvent_;

  QMutex* triggerMutex_;

  bool separateInput_;
  bool ending_;
  bool removeStartCodes_;

  TaskToken currentTask_;

  bool stop_;
  bool noMoreTasks_;
};
