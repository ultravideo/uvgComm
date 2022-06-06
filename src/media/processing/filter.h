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
#include <chrono>

// One of the most fundamental classes of Kvazzup. A filter is an indipendent data processing
// unit running on its own thread. Filters can be linked together to form a data processing pipeline
// called filter graph. A class inherited from filter can do any sort of processing to the data it
// receives. The filter sleeps when it does not have any input to process.

// the numbers will change as new formats are added, please use enum values
enum DataType {DT_NONE        = 0,
               DT_RGB32VIDEO  = (1 << 1),
               DT_YUV420VIDEO = (1 << 2),
               DT_YUYVVIDEO   = (1 << 3),
               DT_HEVCVIDEO   = (1 << 4),
               DT_RAWAUDIO    = (1 << 5),
               DT_OPUSAUDIO   = (1 << 6)};

enum DataSource {DS_UNKNOWN, DS_LOCAL, DS_REMOTE};


struct VideoInfo
{
  int16_t width = 0;
  int16_t height = 0;
  double framerate = 0.0f;

  bool flippedVertically = false;
  bool flippedHorizontally = false;

  int roiWidth = 0;
  int roiHeight = 0;
  std::unique_ptr<int8_t[]> roiArray = nullptr;
};

struct AudioInfo
{
  uint16_t sampleRate = 0;
};

struct Data
{
  DataSource source = DS_UNKNOWN;
  DataType type = DT_NONE;
  std::unique_ptr<uchar[]> data = nullptr;
  uint32_t data_size = 0;

  int64_t presentationTime = -1;

  std::unique_ptr<VideoInfo> vInfo = nullptr;
  std::unique_ptr<AudioInfo> aInfo = nullptr;
};

class StatisticsInterface;
class ResourceAllocator;

class Filter : public QThread
{
  Q_OBJECT

public:
  Filter(QString id, QString name, StatisticsInterface* stats,
         std::shared_ptr<ResourceAllocator> hwResources,
         DataType input, DataType output, bool enforceFramerate = false);
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

  std::unique_ptr<Data> initializeData(enum DataType type, DataSource source);

  std::unique_ptr<Data> normalizeOrientation(std::unique_ptr<Data> video,
                                             bool forceHorizontalFlip = false);

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

  std::shared_ptr<ResourceAllocator> getHWManager() const
  {
    return hwResources_;
  }

  // -1 disables buffer, but its not recommended because delay
  int maxBufferSize_;

  DataType input_;
  DataType output_;

  bool isVideo(DataType type);
  bool isAudio(DataType type);

  void printDataBytes(QString type, uint8_t *payload, size_t size,
                      int bytes, int shift);

private:

  std::unique_ptr<Data> validityCheck(std::unique_ptr<Data> data, bool &ok);

  std::chrono::time_point<std::chrono::high_resolution_clock> getFrameTimepoint();
  void resetSynchronizationPoint(double framerate);

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

  std::shared_ptr<ResourceAllocator> hwResources_;

  uint32_t filterID_;

  // optional smoothing of input frames to frame rate
  bool enforceFramerate_;
  std::chrono::time_point<std::chrono::high_resolution_clock> synchronizationPoint_;
  uint64_t framesSinceSynchronization_;
  double currentFramerate_;
};
