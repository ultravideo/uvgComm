#pragma once

#include <QWaitCondition>
#include <QThread>
#include <QMutex>

#include <sys/time.h>
#include <cstdint>
#include <vector>
#include <queue>
#include <memory>
#include <functional>

enum DataType {NONE = 0, RGB32VIDEO, YUVVIDEO, RAWAUDIO, HEVCVIDEO, OPUSAUDIO};

struct Data
{
  uint8_t type;
  std::unique_ptr<uchar[]> data;
  uint32_t data_size;
  int16_t width;
  int16_t height;
  timeval presentationTime;
};

const int BUFFERSIZE = 50;
class StatisticsInterface;

class Filter : public QThread
{
  Q_OBJECT

public:
  Filter(QString name, StatisticsInterface* stats, bool input, bool output);
  virtual ~Filter();

  // adds one outbound connection to this filter.
  void addOutConnection(Filter *out);

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
  virtual bool isInputFilter() const
  {
    return input_;
  }
  virtual bool isOutputFilter() const
  {
    return output_;
  }

  virtual void stop();

protected:

  // return: oldest element in buffer, empty if none found
  std::unique_ptr<Data> getInput();

  //sends output to out connections
  void sendOutput(std::unique_ptr<Data> output);

  // QThread function that runs the processing
  void run();

  virtual void process() = 0;

  void sleep()
  {
    waitMutex_->lock();
    hasInput_.wait(waitMutex_);
    waitMutex_->unlock();
  }

  QString name_;
  StatisticsInterface* stats_;


private:

  Data* deepDataCopy(Data* original);

  QMutex *waitMutex_;
  QWaitCondition hasInput_;

  bool running_;

  std::vector<std::function<void(std::unique_ptr<Data>)> > outDataCallbacks_;

  std::vector<Filter*> outConnections_;

  QMutex bufferMutex_;
  std::queue<std::unique_ptr<Data>> inBuffer_;

  unsigned int inputTaken_;
  unsigned int inputDiscarded_;

  bool input_;
  bool output_;
};
