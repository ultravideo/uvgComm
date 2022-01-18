#include "filter.h"

#include "statisticsinterface.h"

#include "common.h"
#include "logger.h"

#include <QDebug>

Filter::Filter(QString id, QString name, StatisticsInterface *stats,
               std::shared_ptr<HWResourceManager> hwResources,
               DataType input, DataType output):
  maxBufferSize_(10),
  input_(input),
  output_(output),
  name_(name),
  id_(id),
  stats_(stats),
  waitMutex_(new QMutex),
  hasInput_(),
  running_(true),
  inputTaken_(0),
  inputDiscarded_(0),
  hwResources_(hwResources),
  filterID_(0)
{
  Q_ASSERT(hwResources != nullptr);
}

Filter::~Filter()
{
  delete waitMutex_;
}

void Filter::updateSettings()
{}

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
    Logger::getLogger()->printDebug(DEBUG_WARNING, this,
                                    "Did not succeed at removing outconnection.");
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
    Logger::getLogger()->printDebug(DEBUG_WARNING, this,  "Discarding bad data.");
    return;
  }

  ++inputTaken_;

  bufferMutex_.lock();

  if(inputTaken_%30 == 0)
  {
    stats_->updateBufferStatus(filterID_, (uint16_t)inBuffer_.size(), maxBufferSize_);
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
        if(!isHEVCIntra(buff))
        {
          Logger::getLogger()->printWarning(this, "Discarding HEVC frames from buffer. Finding next intra",
                                            "Frames discarded", QString::number(i));

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
        Logger::getLogger()->printDebug(DEBUG_WARNING, this,  
                                        "Should input Null pointer to opus decoder.");
      }
      inBuffer_.pop_front(); // discard the oldest
    }

    ++inputDiscarded_;
    stats_->packetDropped(filterID_);

    if (inputDiscarded_ == 1 || inputDiscarded_%10 == 0)
    {
      Logger::getLogger()->printDebug(DEBUG_WARNING, this, "Buffer too full",
                                      {"Name", "Discarded/total input"},
                                      {name_, QString::number(inputDiscarded_) + "/" +
                                              QString::number(inputTaken_)});
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
    Logger::getLogger()->printDebug(DEBUG_WARNING, this, 
                                    "Trying to send output data without outconnections.");
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
  filterID_ = stats_->addFilter(name_, id_, (uint64_t)currentThreadId());
  while(running_)
  {
    waitForInput();
    if(!running_) break;

    process();
  }
  if (filterID_ != 0)
  {
    stats_->removeFilter(filterID_);
    filterID_ = 0;
  }
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
  Logger::getLogger()->printDebug(DEBUG_WARNING, this, 
                                  "Trying to copy nullptr Data pointer.");
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
  Logger::getLogger()->printDebug(DEBUG_WARNING, this, 
                                  "Trying to copy nullptr Data pointer.");
  return nullptr;
}


QString Filter::printOutputs()
{
  QString outs = "";

  for(auto& out : outConnections_)
  {
    outs += "   \"" + name_ + "\" -> \"" + out->name_ + "\";" + "\r\n";
  }

  outs += "plus " + QString::number(outDataCallbacks_.size()) + " callbacks";
  return outs;
}


bool Filter::isHEVCIntra(const unsigned char *buff)
{
  return (buff[0] == 0 &&
      buff[1] == 0 &&
      buff[2] == 0 &&
      buff[3] == 1 &&
      (buff[4] >> 1) == 1);
}
