#include "filter.h"

#include "statisticsinterface.h"

#include "common.h"

#include <QDebug>

Filter::Filter(QString id, QString name, StatisticsInterface *stats,
               DataType input, DataType output):
  name_(id + name),
  stats_(stats),
  maxBufferSize_(10),
  input_(input),
  output_(output),
  waitMutex_(new QMutex),
  hasInput_(),
  running_(true),
  inputTaken_(0),
  inputDiscarded_(0)
{}

Filter::~Filter()
{
  delete waitMutex_;
}

void Filter::updateSettings()
{
  // use the following in inherited code. This way the config doesn't go to registry.
  // QSettings settings("kvazzup.ini", QSettings::IniFormat);

  // get value with
  //int value = settings.value("valuename").toInt();
}

bool Filter::init()
{
  return true;
}

void Filter::addOutConnection(std::shared_ptr<Filter> out)
{
  outConnections_.push_back(out);
}

void Filter::removeOutConnection(std::shared_ptr<Filter> out)
{
  bool removed = false;
  connectionMutex_.lock();
  for(unsigned int i = 0; i < outConnections_.size(); ++i)
  {
    if(outConnections_[i].get() == out.get())
    {
      outConnections_.erase(outConnections_.begin() + i);
      removed = true;
      break;
    }
  }
  connectionMutex_.unlock();

  if(!removed)
  {
    printDebug(DEBUG_WARNING, this, "Uninit", "Did not succeed at removing outconnection.");
  }
}

void Filter::emptyBuffer()
{
  bufferMutex_.lock();
  //std::queue<std::unique_ptr<Data>> empty;
  std::deque<std::unique_ptr<Data>> empty;
  std::swap( inBuffer_, empty );
  bufferMutex_.unlock();
}

void Filter::putInput(std::unique_ptr<Data> data)
{
  Q_ASSERT(data);

  if(data->source == UNKNOWN
     || data->type == NONE
     || data->data_size == 0)
  {
    printDebug(DEBUG_WARNING, this, "Processing", "Discarding bad data.");
    return;
  }

  ++inputTaken_;

  bufferMutex_.lock();

  if(inputTaken_%30 == 0)
  {
    stats_->updateBufferStatus(getName(), inBuffer_.size(), maxBufferSize_);
  }

  inBuffer_.push_back(std::move(data));

  if(maxBufferSize_ != -1 && inBuffer_.size() >= (uint32_t)maxBufferSize_)
  {
    if(inBuffer_[0]->type == HEVCVIDEO)
    {
      // Search for intra frames and discard everything up to it
      for(uint32_t i = 0; i < inBuffer_.size(); ++i)
      {
        const unsigned char *buff = inBuffer_.at(i)->data.get();
        if(!(buff[0] == 0
           && buff[1] == 0
           && buff[2] == 0
           && buff[3] == 1
           && (buff[4] >> 1) == 1))
        {
          qDebug() << "Processing," << metaObject()->className() << ": Discarding" << i
                   << "HEVC frames. Found non inter frame from buffer at :" << i;
          for(int j = i; j != 0; --j)
          {
            inBuffer_.pop_front();
          }
          break;
        }
      }
    }
    else
    {
      if(inBuffer_[0]->type == OPUSAUDIO)
      {
        printDebug(DEBUG_WARNING, this, "Processing", "Should input Null pointer to decoder.");
      }
      inBuffer_.pop_front(); // discard the oldest
    }

    ++inputDiscarded_;
    stats_->packetDropped(name_);
    if(inputDiscarded_ == 1 || inputDiscarded_%10 == 0)
    {
      qDebug() << "Processing," << name_ << "buffer full. Discarded input:"
               << inputDiscarded_  << "Total input:" << inputTaken_;
    }
  }
  wakeUp();

  bufferMutex_.unlock();
}

std::unique_ptr<Data> Filter::getInput()
{
  bufferMutex_.lock();
  std::unique_ptr<Data> r;
  if(!inBuffer_.empty())
  {
    r = std::move(inBuffer_.front());
    inBuffer_.pop_front();
  }
  bufferMutex_.unlock();
  return r;
}

void Filter::sendOutput(std::unique_ptr<Data> output)
{
  Q_ASSERT(output);

  if(outDataCallbacks_.size() == 0 && outConnections_.size() == 0)
  {
    printDebug(DEBUG_WARNING, this, "Processing", "Trying to send output data without outconnections.");
    return;
  }

  connectionMutex_.lock();
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

    // copy last callback and move last connection
    if(outConnections_.size() != 0)
    {
      Data* copy = deepDataCopy(output.get());
      std::unique_ptr<Data> u_copy(copy);
      outDataCallbacks_.back()(std::move(u_copy));
    }
    else // move last callback
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
  connectionMutex_.unlock();
}

void Filter::stop()
{
  running_ = false;
  hasInput_.wakeAll();
}

void Filter::run()
{
  stats_->addFilter(name_, (uint64_t)currentThreadId());
  //qDebug() << "Iniating," << metaObject()->className()
    //<< Running filter" << name_ << "with max buffer:" << maxBufferSize_;
  while(running_)
  {
    waitForInput();
    if(!running_) break;

    process();
  }

  stats_->removeFilter(getName());
}

Data* Filter::shallowDataCopy(Data* original)
{
  if(original != nullptr)
  {
    Data* copy = new Data;
    copy->type = original->type;
    copy->width = original->width;
    copy->height = original->height;
    copy->source = original->source;
    copy->presentationTime = original->presentationTime;
    copy->framerate = original->framerate;
    copy->data_size = 0; // no data in shallow copy

    return copy;
  }
  printDebug(DEBUG_WARNING, this, "Processing", "Trying to copy nullptr Data pointer.");
  return nullptr;
}

Data* Filter::deepDataCopy(Data* original)
{
  if(original != nullptr)
  {
    Data* copy = shallowDataCopy(original);
    copy->data = std::unique_ptr<uchar[]>(new uchar[original->data_size]);
    memcpy(copy->data.get(), original->data.get(), original->data_size);
    copy->data_size = original->data_size;

    return copy;
  }
  printDebug(DEBUG_WARNING, this, "Processing", "Trying to copy nullptr Data pointer.");
  return nullptr;
}


QString Filter::printOutputs()
{
  QString outs = "";

  for(auto out : outConnections_)
  {
    outs += "   \"" + name_ + "\" -> \"" + out->name_ + "\";" + "\r\n";
  }
  for(auto out : outDataCallbacks_)
  {
    outs += "   \"" + name_ + "\" -> \" All_outputs \";" + "\r\n";
  }
  return outs;
}
