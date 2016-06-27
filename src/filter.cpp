#include "filter.h"

#include <QDebug>

Filter::Filter():running_(true), waitMutex_(NULL),
  inputTaken_(0), inputDiscarded_(0)
{
  waitMutex_ = new QMutex;
}


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
  if(inBuffer_.size() < BUFFERSIZE)
    inBuffer_.push(std::move(data));
  else
  {
    ++inputDiscarded_;
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

  for(unsigned int i = 0; i < outConnections_.size() - 1; ++i)
  {
    Data* copy = new Data;
    copy->type = output->type;
    copy->data = std::unique_ptr<uchar[]>(new uchar[output->data_size]);
    memcpy(copy->data.get(), output->data.get(), output->data_size);
    copy->data_size = output->data_size;
    copy->width = output->width;
    copy->height = output->height;

    std::unique_ptr<Data> u_copy(copy);
    outConnections_[i]->putInput(std::move(u_copy));
  }
  if(!outConnections_.empty())
    outConnections_.back()->putInput(std::move(output));
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
