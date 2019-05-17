#pragma once
#include "media/processing/filter.h"
#include <FramedSource.hh>
#include <QMutex>
#include <QSemaphore>

#include "filter.h"
#include "../rtplib/src/writer.hh"

class StatisticsInterface;

class FramedSourceFilter : public Filter
{
public:
  FramedSourceFilter(QString id, StatisticsInterface *stats, DataType type, QString media, RTPWriter *writer);
  ~FramedSourceFilter();

  void updateSettings();

  void start();
  void stop();

  void copyFrameToBuffer(std::unique_ptr<Data> currentFrame);

protected:
  void process();

private:
  DataType type_; // TODO use output_
  bool stop_;
  bool separateInput_;
  bool removeStartCodes_;

  RTPWriter *writer_;
  uint64_t frame_;
  rtp_format_t dataFormat_;
};

#if 0
/* #ifdef _WIN32 */
class FramedSourceFilter : public Filter
{
public:
  FramedSourceFilter(QString id, StatisticsInterface *stats,
                     DataType type, QString media, QMutex *triggerMutex,
                     bool live555Copying);

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
#endif
