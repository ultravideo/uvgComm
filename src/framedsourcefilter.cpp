#include "framedsourcefilter.h"

#include <QTime>
#include <QDebug>


FramedSourceFilter::FramedSourceFilter(UsageEnvironment &env, DataType type):
  FramedSource(env),
  type_(type)
//  eventTriggerId_(0),
//  referenceCount_(0)
{
 // eventTriggerId_ = envir().taskScheduler().createEventTrigger(deliverFrame0);
}

void FramedSourceFilter::doGetNextFrame()
{
  qDebug() << "doGetNextFrame()";
  std::unique_ptr<Data> frame = getInput();

  fFrameSize = 0;
  if(frame)
  {
    qDebug() << "Sending frame at sec since epoch:" << QDateTime::currentMSecsSinceEpoch()/1000;
    timeval present_time;
    present_time.tv_sec = 0;
    present_time.tv_usec = 0;
    //QTime t = QTime::currentTime();
    present_time.tv_sec = QDateTime::currentMSecsSinceEpoch()/1000;
   // present_time.tv_usec = (QDateTime::currentMSecsSinceEpoch()%1000) * 1000;
    fPresentationTime = present_time;

    fDurationInMicroseconds = 1000000/15;

    //gettimeofday(&fPresentationTime, NULL);

    qDebug() << "Copying frame:" << frame->data_size << "/" << fMaxSize;

    if(frame->data_size > fMaxSize)
    {
      fFrameSize = fMaxSize;
      fNumTruncatedBytes = frame->data_size - fMaxSize;
    }
    else
    {
      fFrameSize = frame->data_size;
    }

    memcpy(fTo, frame->data.get(), fFrameSize);
  }

  nextTask() = envir().taskScheduler().scheduleDelayedTask(1,
  (TaskFunc*)FramedSource::afterGetting, this);
  qDebug() << "afterGetting";
}

void FramedSourceFilter::process()
{}
