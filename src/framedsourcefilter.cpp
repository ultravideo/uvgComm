#include "framedsourcefilter.h"

#include <QTime>
#include <QDebug>


FramedSourceFilter::FramedSourceFilter(StatisticsInterface* stats,
                                       UsageEnvironment &env, DataType type):
  Filter("Framed source", stats),
  FramedSource(env),
  type_(type)
{}

void FramedSourceFilter::doGetNextFrame()
{
  std::unique_ptr<Data> frame = getInput();

  fFrameSize = 0;
  if(frame)
  {
    //qDebug() << "Sending frame at sec since epoch:" << QDateTime::currentMSecsSinceEpoch()/1000
    //         << "Framesize:" << frame->data_size << "/" << fMaxSize;
    timeval present_time;
    present_time.tv_sec = 0;
    present_time.tv_usec = 0;
    present_time.tv_sec = QDateTime::currentMSecsSinceEpoch()/1000;
    present_time.tv_usec = (QDateTime::currentMSecsSinceEpoch()%1000) * 1000;
    fPresentationTime = present_time;

    fDurationInMicroseconds = 1000000/30;

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
}

void FramedSourceFilter::process()
{}
