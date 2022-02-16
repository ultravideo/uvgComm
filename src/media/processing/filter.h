#pragma once

#include <QWaitCondition>
#include <QThread>
#include <QMutex>

#ifndef _MSC_VER
#include <sys/time.h>
#else
#include <WinSock2.h>
#endif

#include <cstdint>
#include <vector>
#include <queue>
#include <memory>
#include <functional>

// One of the most fundamental classes of Kvazzup. A filter is an indipendent data processing
// unit running on its own thread. Filters can be linked together to form a data processing pipeline
// called filter graph. A class inherited from filter can do any sort of processing to the data it
// receives. The filter sleeps when it does not have any input to process.

enum DataType {NONE = 0, RGB32VIDEO, YUV420VIDEO, YUYVVIDEO, RAWAUDIO, HEVCVIDEO, OPUSAUDIO};
enum DataSource {UNKNOWN, LOCAL, REMOTE};

// NOTE: remember to make changes to deepcopy, camera, audiocapture and RTP Receiver filter
struct Data
{
  uint8_t type;
  std::unique_ptr<uchar[]> data;
  uint32_t data_size;
  int16_t width;
  int16_t height;
  int64_t presentationTime;

  uint16_t framerate;

  DataSource source;
};

class StatisticsInterface;
class HWResourceManager;

class Filter : public QThread
{
  Q_OBJECT

public:
  Filter(QString id, QString name, StatisticsInterface* stats,
         std::shared_ptr<HWResourceManager> hwResources,
         DataType input, DataType output);
  virtual ~Filter();

  // Redefine this to handle the initialization of the filter
  virtual bool init();

  // the settings have been updated. Redefine this if inherited class uses qsettings.
  virtual void updateSettings();

  // adds one outbound connection to this filter.
  void addOutConnection(std::shared_ptr<Filter> out);
  void removeOutConnection(std::shared_ptr<Filter> out);

  // callback registeration enables other classes besides Filter
  // to receive output data
  template <typename Class>
  void addDataOutCallback (Class* o, void (Class::*method) (std::unique_ptr<Data> data))
  {
    outDataCallbacks_.push_back([o,method] (std::unique_ptr<Data> data)
    {
      return (o->*method)(std::move(data));
    });
  }

  // empties the input buffer
  void emptyBuffer();

  void putInput(std::unique_ptr<Data> data);

  // for debugging filter graphs
  virtual DataType inputType() const
  {
    return input_;
  }
  virtual DataType outputType() const
  {
    return output_;
  }

  virtual void start()
  {
    running_ = true;
    QThread::start();
  }

  virtual void stop();

  QString printOutputs();

  // helper function for copying Data
  Data* shallowDataCopy(Data* original);
  Data* deepDataCopy(Data* original);

  QString getName()
  {
    return name_;
  }

protected:

  // return: oldest element in buffer, empty if none found
  std::unique_ptr<Data> getInput();

  //sends output to out connections
  void sendOutput(std::unique_ptr<Data> output);

  // QThread function that runs the processing
  void run();

  virtual void process() = 0;

  bool isHEVCIntra(const unsigned char *buff);
  bool isHEVCInter(const unsigned char *buff);

  void wakeUp()
  {
    waitMutex_->lock();
    hasInput_.wakeOne();
    waitMutex_->unlock();
  }

  void waitForInput()
  {
    waitMutex_->lock();
    // unlocks the mutex
    hasInput_.wait(waitMutex_);
    waitMutex_->unlock();
  }

  StatisticsInterface* getStats()
  {
    Q_ASSERT(stats_);
    return stats_;
  }

  std::shared_ptr<HWResourceManager> getHWManager() const
  {
    return hwResources_;
  }

  // -1 disables buffer, but its not recommended because delay
  int maxBufferSize_;

  DataType input_;
  DataType output_;

private:

  QString name_;
  QString id_;

  StatisticsInterface* stats_;
  QMutex *waitMutex_;
  QWaitCondition hasInput_;

  bool running_;

  std::vector<std::function<void(std::unique_ptr<Data>)> > outDataCallbacks_;

  QMutex connectionMutex_;
  std::vector<std::shared_ptr<Filter>> outConnections_;

  QMutex bufferMutex_;
  //std::queue<std::unique_ptr<Data>> inBuffer_;
  std::deque<std::unique_ptr<Data>> inBuffer_;

  unsigned int inputTaken_;
  unsigned int inputDiscarded_;

  std::shared_ptr<HWResourceManager> hwResources_;

  uint32_t filterID_;
};
