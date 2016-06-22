#include "framedsourcepile.h"

#include "filter.h"

#include <QTime>
#include <QDebug>


FramedSourcePile::FramedSourcePile(UsageEnvironment &env):
  FramedSource(env)//,
//  eventTriggerId_(0),
//  referenceCount_(0)
{
 // eventTriggerId_ = envir().taskScheduler().createEventTrigger(deliverFrame0);
}

void FramedSourcePile::doGetNextFrame()
{


  std::unique_ptr<Data> frame = getInput();
  if(frame)
  {
      qDebug() << "Sending frame";
    timeval present_time;

    QTime t = QTime::currentTime();

    present_time.tv_sec = t.msecsSinceStartOfDay()/1000;
    present_time.tv_usec = (t.msecsSinceStartOfDay()%1000) * 1000;
    fPresentationTime = present_time;

    //gettimeofday(&fPresentationTime, NULL);

    memcpy(fTo, frame->data.get(), frame->data_size);

    FramedSource::afterGetting(this);
  }
}

// TODO duplicated code. Combine with filter code
void FramedSourcePile::putInput(std::unique_ptr<Data> data)
{
  Q_ASSERT(data);

  pileMutex_.lock();
  if(pile_.size() < BUFFERSIZE)
    pile_.push(std::move(data));
  pileMutex_.unlock();

  qDebug() << "Framed source pile size:" << pile_.size();
}

std::unique_ptr<Data> FramedSourcePile::getInput()
{
  pileMutex_.lock();
  std::unique_ptr<Data> r;
  if(!pile_.empty())
  {
    r = std::move(pile_.front());
    pile_.pop();
  }
  pileMutex_.unlock();
  return r;
}
