#include "filter.h"

#include "statisticswindow.h"

#include <QDebug>

Filter::Filter(QString name, StatisticsInterface *stats,
               bool input, bool output):
  name_(name),
  stats_(stats),
  waitMutex_(new QMutex),
  hasInput_(),
  running_(true),
  inputTaken_(0),
  inputDiscarded_(0),
  input_(input),
  output_(output)
{}

Filter::~Filter()
{
  delete waitMutex_;
}

void Filter::addOutConnection(Filter *out)
{
  outConnections_.push_back(out);
}

void Filter::emptyBuffer()
{
  bufferMutex_.lock();
  std::queue<std::unique_ptr<Data>> empty;
  std::swap( inBuffer_, empty );
  bufferMutex_.unlock();
}

void Filter::putInput(std::unique_ptr<Data> data)
{
  Q_ASSERT(data);

  ++inputTaken_;

  bufferMutex_.lock();

  if(inputTaken_%30 == 0)
  {
    stats_->updateBufferStatus(name_, inBuffer_.size());
    //qDebug() << name_ << "buffer status:" << inBuffer_.size();
  }

  if(inBuffer_.size() < BUFFERSIZE)
    inBuffer_.push(std::move(data));
  else
  {
    ++inputDiscarded_;
    stats_->packetDropped();
    qDebug() << name_ << " buffer full. Discarded input: "
             << inputDiscarded_
             << " Total input: "
             << inputTaken_;
  }
  hasInput_.wakeOne();
  bufferMutex_.unlock();
}

std::unique_ptr<Data> Filter::getInput()
{
  bufferMutex_.lock();
  std::unique_ptr<Data> r;
  if(!inBuffer_.empty())
  {
    r = std::move(inBuffer_.front());
    inBuffer_.pop();
  }
  bufferMutex_.unlock();
  return r;
}

void Filter::sendOutput(std::unique_ptr<Data> output)
{
  Q_ASSERT(output);

  if(outDataCallbacks_.size() == 0 && outConnections_.size() == 0)
  {
    qWarning() << name_ << "trying to send output data without outconnections";
    return;
  }

  // copy data to callbacks expect the last one is moved
  // in either callbacks or outconnections(default).
  if(outDataCallbacks_.size() != 0)
  {
    // all expect the last
    for(unsigned int i = 0; i < outDataCallbacks_.size() - 1; ++i)
    {
      Data* copy = deepDataCopy(output.get());
      std::unique_ptr<Data> u_copy(copy);
      outDataCallbacks_[i](std::move(u_copy));
    }

    // copy last one
    if(outConnections_.size() != 0)
    {
      Data* copy = deepDataCopy(output.get());
      std::unique_ptr<Data> u_copy(copy);
      outDataCallbacks_.back()(std::move(u_copy));
    }
    else // move last one
    {
      outDataCallbacks_.back()(std::move(output));
    }
  }

  // handle all connected filters.
  if(outConnections_.size() != 0)
  {
    // all expect the last
    for(unsigned int i = 0; i < outConnections_.size() - 1; ++i)
    {
      Data* copy = deepDataCopy(output.get());
      std::unique_ptr<Data> u_copy(copy);
      outConnections_[i]->putInput(std::move(u_copy));
    }
    // always move the last outconnection
    outConnections_.back()->putInput(std::move(output));
  }
}

void Filter::stop()
{
  running_ = false;
  hasInput_.wakeAll();
}

void Filter::run()
{
  running_ = true;
  while(running_)
  {
    sleep();
    if(!running_) break;

    process();
  }
}

Data* Filter::deepDataCopy(Data* original)
{
  if(original != NULL)
  {
    Data* copy = new Data;
    copy->type = original->type;
    copy->data = std::unique_ptr<uchar[]>(new uchar[original->data_size]);
    memcpy(copy->data.get(), original->data.get(), original->data_size);
    copy->data_size = original->data_size;
    copy->width = original->width;
    copy->height = original->height;
    return copy;
  }
  qWarning() << "Warning: Trying to copy NULL Data pointer";
  return NULL;
}
