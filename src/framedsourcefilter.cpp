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
  afterEvent_(0),
  separateInput_(!live555Copying),
  //separateInput_(false),
  triggerMutex_(triggerMutex),
  ending_(false),
  removeStartCodes_(false),
  currentTask_(),
  stop_(false), // whether we should stop. Called when stopping
  noMoreTasks_(true)
{
  updateSettings();
}

FramedSourceFilter::~FramedSourceFilter()
{}

void FramedSourceFilter::updateSettings()
{
  // called in case we later decide to add some settings to filter
  Filter::updateSettings();

  if(type_ == HEVCVIDEO)
  {
    QSettings settings("kvazzup.ini", QSettings::IniFormat);
    uint32_t vps = settings.value("video/VPS").toInt();
    uint16_t intra = settings.value("video/Intra").toInt();

    if(settings.value("video/Slices").toInt() == 1)
    {
      // 4k 30 fps uses can fit to max 200 slices.
      int maxSlices = 200;
      // this buffersize is intended to hold at least one intra frame so we can
      // delete all picture that do not belong to it. TODO: not implemented.
      maxBufferSize_ = maxSlices*vps*intra;
      removeStartCodes_ = false;
    }
    else
    {
      // the number of frames between parameter sets
      maxBufferSize_ = vps*intra;

      // discrete framer doesn't like start codes.
      removeStartCodes_ = true;
    }

     qDebug() << name_ << "updated buffersize to" << maxBufferSize_;

     if(settings.value("video/liveCopying").isValid())
     {
       separateInput_ = (settings.value("video/liveCopying").toInt() == 1);
     }
     else
     {
       qDebug() << "ERROR: Missing settings value flip threads";
     }
  }
}

void FramedSourceFilter::doGetNextFrame()
{
  // The fTo pointer is actually given as a pointer to be put data
  // so replacing the pointer does nothing.

  // TODO_BUG: There is a possibility of timeout under heavy load (such as starting the filter)
  // for a timeout which causes getnextframe to be called a second time resulting in an internal error
  // for live555

  if(isCurrentlyAwaitingData())
  {
    if(separateInput_)
    {
      framePointerReady_.release();
    }
    else if(!stop_)
    {
      std::unique_ptr<Data> currentFrame = getInput();
      copyFrameToBuffer(std::move(currentFrame));
      envir().taskScheduler().scheduleDelayedTask(1, (TaskFunc*)FramedSource::afterGetting, this);
    }
  }
  else
  {
    fFrameSize = 0;
    currentTask_ = envir().taskScheduler().scheduleDelayedTask(1, (TaskFunc*)FramedSource::afterGetting, this);
  }
}

void FramedSourceFilter::start()
{
  if(separateInput_)
  {
    afterEvent_ = envir().taskScheduler().createEventTrigger((TaskFunc*)FramedSource::afterGetting);
    qDebug() << "Creating trigger for framedSource:" << name_ << "Trigger ID: " << afterEvent_;
  }

  Filter::start();

  noMoreTasks_ = false;
}

void FramedSourceFilter::stop()
{
  stopGettingFrames();
  Filter::stop();
  stop_ = true;
  framePointerReady_.release();

  uint16_t waitTime = 5;

  while(!noMoreTasks_)
  {
    qSleep(waitTime);
    qDebug() << "Waiting for no more tasks";
  }

  if(afterEvent_)
  {
    qDebug() << "Removing trigger from live555:" << afterEvent_;
    envir().taskScheduler().deleteEventTrigger(afterEvent_);
    afterEvent_ = 0;
  }

  if(currentTask_)
  {
    qDebug() << "Unscheduling delayed task from live555:" << currentTask_;
    envir().taskScheduler().unscheduleDelayedTask(currentTask_);
    currentTask_ = 0;
  }

}

void FramedSourceFilter::process()
{
  // There is no way to copy the data here, because the
  // pointer is given only after doGetNextFrame is called
  while(separateInput_)
  {
    framePointerReady_.acquire(1);

    if(stop_)
    {
      return;
    }

    std::unique_ptr<Data> currentFrame = getInput();

    if(currentFrame == nullptr)
    {
      fFrameSize = 0;
      sendFrame();
      return;
    }
    while(currentFrame)
    {
      copyFrameToBuffer(std::move(currentFrame));
      sendFrame();

      currentFrame = nullptr;

      framePointerReady_.acquire(1);
      if(stop_)
      {
        return;
      }
      currentFrame = getInput();
      // copy additional NAL units, if available.
      if(currentFrame == nullptr)
      {
        fFrameSize = 0;
        sendFrame();
        return;
      }
    }
  }
}

void FramedSourceFilter::sendFrame()
{
  // trigger the live555 to send the copied NAL unit.
  triggerMutex_->lock();
  envir().taskScheduler().triggerEvent(afterEvent_, this);
  triggerMutex_->unlock();
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


void FramedSourceFilter::doStopGettingFrames()
{
  FramedSource::doStopGettingFrames();
  noMoreTasks_ = true;
}
