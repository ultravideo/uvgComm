#include "framedsourcefilter.h"

#include "statisticsinterface.h"

#include <QDebug>


FramedSourceFilter::FramedSourceFilter(StatisticsInterface* stats,
                                       UsageEnvironment &env, DataType type, uint16_t framerate):
  Filter("Framed source", stats, true, false),
  FramedSource(env),
  type_(type),
  framerate_(framerate)
{}

void FramedSourceFilter::doGetNextFrame()
{
  std::unique_ptr<Data> frame = getInput();

  fFrameSize = 0;
  if(frame)
  {
    //qDebug() << "Sending frame at sec since epoch:" << QDateTime::currentMSecsSinceEpoch()/1000
    //         << "Framesize:" << frame->data_size << "/" << fMaxSize;

    fPresentationTime = frame->presentationTime;
    fDurationInMicroseconds = 1000000/framerate_;

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
    stats_->addSendPacket(frame->data_size);
  }

  nextTask() = envir().taskScheduler().scheduleDelayedTask(1,
  (TaskFunc*)FramedSource::afterGetting, this);
}

void FramedSourceFilter::process()
{}
