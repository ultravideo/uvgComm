#include "framedsourcefilter.h"

#include "statisticsinterface.h"

#include "common.h"

#include <UsageEnvironment.hh>

#include <QSettings>
#include <QDebug>


FramedSourceFilter::FramedSourceFilter(QString id, StatisticsInterface* stats,
                                       UsageEnvironment &env, DataType type, QString media):
  FramedSource(env),
  Filter(id, "Framed_Source_" + media, stats, type, NONE),
  type_(type),
  afterEvent_(),
  separateInput_(true)
{
  updateSettings();
  afterEvent_ = envir().taskScheduler().createEventTrigger((TaskFunc*)FramedSource::afterGetting);
  qDebug() << "Creating trigger:" << afterEvent_;

  maxBufferSize_ = -1;
}

void FramedSourceFilter::updateSettings()
{
  // called in case we later decide to add some settings to filter
  Filter::updateSettings();

  if(type_ == HEVCVIDEO)
  {
    QSettings settings("kvazzup.ini", QSettings::IniFormat);
    if(settings.value("video/Slices").toInt() == 1)
    {
      maxBufferSize_ = -1; // no buffer limit
    }
    else
    {
      // the number of frames between parameter sets
      maxBufferSize_ = settings.value("video/VPS").toInt()
                      *settings.value("video/Intra").toInt();
    }

     qDebug() << name_ << "updated buffersize to" << maxBufferSize_;
  }
}

void FramedSourceFilter::doGetNextFrame()
{
  // The fTo pointer is actually given as a pointer to be put data
  // so replacing the pointer does nothing.
  if(separateInput_)
  {
    framePointerReady_.release();
  }
  else
  {
    std::unique_ptr<Data> currentFrame = getInput();
    copyFrameToBuffer(std::move(currentFrame));
    envir().taskScheduler().scheduleDelayedTask(1, (TaskFunc*)FramedSource::afterGetting, this);
  }
}

void FramedSourceFilter::process()
{
  // There is no way to copy the data here, because the
  // pointer is given only after doGetNextFrame is called
  if(separateInput_ && framePointerReady_.tryAcquire(1))
  {
    std::unique_ptr<Data> currentFrame = getInput();

    if(currentFrame == NULL)
    {
      framePointerReady_.release();
      qDebug() << "Releasing because input was not available. Available:" << framePointerReady_.available();
    }

    while(currentFrame)
    {
      copyFrameToBuffer(std::move(currentFrame));
      currentFrame = NULL;
      // trigger the live555 to send the copied frame.
      envir().taskScheduler().triggerEvent(afterEvent_, this);

      if(framePointerReady_.tryAcquire(1))
      {
        currentFrame = getInput();
        if(currentFrame == NULL)
        {
          framePointerReady_.release();
          qDebug() << "Releasing because input not available. Available:" << framePointerReady_.available();
        }
        else
        {
          qDebug() << "Processing additional frames:" << name_ << "Available:" << framePointerReady_.available();
        }
      }
      else
      {
        currentFrame = NULL;
      }
    }
  }
}

void FramedSourceFilter::copyFrameToBuffer(std::unique_ptr<Data> currentFrame)
{
  fFrameSize = 0;
  if(currentFrame)
  {
    fPresentationTime = currentFrame->presentationTime;
    if(currentFrame->framerate != 0)
    {
      //1000000/framerate is the correct length but
      // 0 works with slices
      fDurationInMicroseconds = 0;
    }

    if(currentFrame->data_size > fMaxSize)
    {
      fFrameSize = fMaxSize;
      fNumTruncatedBytes = currentFrame->data_size - fMaxSize;
      qDebug() << "WARNING: Requested sending larger packet than possible:"
               << currentFrame->data_size << "/" << fMaxSize;
    }
    else
    {
      fFrameSize = currentFrame->data_size;
      fNumTruncatedBytes = 0;
    }

    memcpy(fTo, currentFrame->data.get(), fFrameSize);

    stats_->addSendPacket(fFrameSize);
  }
}
