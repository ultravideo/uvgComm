#include "framedsourcefilter.h"

#include "statisticsinterface.h"

#include "common.h"

#include <UsageEnvironment.hh>

#include <QSettings>
#include <QDebug>


FramedSourceFilter::FramedSourceFilter(QString id, StatisticsInterface* stats,
                                       UsageEnvironment &env, DataType type, QString media,
                                       QMutex* triggerMutex, bool live555Copying):
  FramedSource(env),
  Filter(id, "Framed_Source_" + media, stats, type, NONE),
  type_(type),
  afterEvent_(),
  //separateInput_(!live555Copying),
  separateInput_(false),
  triggerMutex_(triggerMutex),
  ending_(false),
  removeStartCodes_(true)
{
  updateSettings();
  afterEvent_ = envir().taskScheduler().createEventTrigger((TaskFunc*)FramedSource::afterGetting);
  qDebug() << "Creating trigger for framedSource:" << afterEvent_;
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
  if(isCurrentlyAwaitingData())
  {
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
  else
  {
    fFrameSize = 0;
    envir().taskScheduler().scheduleDelayedTask(1, (TaskFunc*)FramedSource::afterGetting, this);
  }
}

void FramedSourceFilter::process()
{
  // There is no way to copy the data here, because the
  // pointer is given only after doGetNextFrame is called
  while(separateInput_)
  {
    framePointerReady_.acquire(1);
    std::unique_ptr<Data> currentFrame = getInput();

    if(currentFrame == NULL)
    {
      qDebug() << "No input, calling after getting. Available:" << framePointerReady_.available();
      fFrameSize = 0;
      triggerMutex_->lock();
      envir().taskScheduler().triggerEvent(afterEvent_, this);
      triggerMutex_->unlock();
      return;
    }
    while(currentFrame)
    {
      copyFrameToBuffer(std::move(currentFrame));
      currentFrame = NULL;
      // trigger the live555 to send the copied frame.
      triggerMutex_->lock();
      envir().taskScheduler().triggerEvent(afterEvent_, this);
      triggerMutex_->unlock();

      framePointerReady_.acquire(1);
      currentFrame = getInput();
      if(currentFrame == NULL)
      {
        qDebug() << "No input, calling after getting" << name_ << "Available:" << framePointerReady_.available();
        fFrameSize = 0;
        triggerMutex_->lock();
        envir().taskScheduler().triggerEvent(afterEvent_, this);
        triggerMutex_->unlock();
        return;
      }
      else
      {
        qDebug() << "Processing additional frames:" << name_ << "Available:" << framePointerReady_.available();
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

    if(removeStartCodes_ && type_ == HEVCVIDEO)
    {
      fFrameSize -= 4;
      memcpy(fTo, currentFrame->data.get() + 4, fFrameSize);
    }
    else
    {
      memcpy(fTo, currentFrame->data.get(), fFrameSize);
    }

    stats_->addSendPacket(fFrameSize);
  }
}
