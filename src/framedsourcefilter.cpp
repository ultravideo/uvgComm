#include "framedsourcefilter.h"

#include "statisticsinterface.h"

#include <QSettings>
#include <QDebug>


FramedSourceFilter::FramedSourceFilter(QString id, StatisticsInterface* stats,
                                       UsageEnvironment &env, DataType type):
  FramedSource(env),
  Filter(id, "Framed_Source", stats, true, false),
  type_(type)
{
  QSettings settings;

  if(settings.value("video/Slices").toInt() == 1)
  {
    maxBufferSize_ = -1; // no buffer limit
  }
}

void FramedSourceFilter::doGetNextFrame()
{
  std::unique_ptr<Data> frame = getInput();

  fFrameSize = 0;
  if(frame)
  {

    fPresentationTime = frame->presentationTime;
    if(frame->framerate != 0)
    {
      //1000000/framerate is the correct length but
      // 0 works with slices
      fDurationInMicroseconds = 0;
    }

    if(frame->data_size > fMaxSize)
    {
      fFrameSize = fMaxSize;
      fNumTruncatedBytes = frame->data_size - fMaxSize;
      qDebug() << "WARNING: Requested sending larger packet than possible:"
               << frame->data_size << "/" << fMaxSize;
    }
    else
    {
      fFrameSize = frame->data_size;
      fNumTruncatedBytes = 0;
    }

    memcpy(fTo, frame->data.get(), fFrameSize);
    stats_->addSendPacket(frame->data_size);
  }

  nextTask() = envir().taskScheduler().scheduleDelayedTask(1,
  (TaskFunc*)FramedSource::afterGetting, this);
}

void FramedSourceFilter::process()
{}
