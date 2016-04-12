#pragma once

#include <QWaitCondition>
#include <QThread>
#include <QMutex>

#include <cstdint>
#include <vector>
#include <queue>
#include <memory>

enum DataType {RPG32VIDEO = 0, RAWAUDIO, HEVCVIDEO, OPUSAUDIO};

struct Data
{
  uint8_t type;
  std::unique_ptr<uchar[]> data;
  uint32_t data_size;
  int width;
  int height;
};

class Filter : public QThread
{

  Q_OBJECT

public:
  Filter();
  virtual ~Filter();

  // adds one outbound connection to this filter.
  void addOutconnection(Filter *out);

  // empties the input buffer
  void emptyBuffer();

  void putInput(std::unique_ptr<Data> data);

  // for debug information only
  virtual bool canHaveInputs() const = 0;
  virtual bool canHaveOutputs() const = 0;

  void stop();

protected:

  // return: oldest element in buffer, empty if none found
  std::unique_ptr<Data> getInput();
  void putOutput(std::unique_ptr<Data> data);

  // QThread function that runs the processing itself
  void run() = 0;

  void sleep()
  {
    waitMutex_->lock();
    hasInput_.wait(waitMutex_);
    waitMutex_->unlock();
  }

  bool running_;

private:
  QMutex *waitMutex_;
  QWaitCondition hasInput_;

  std::vector<Filter*> outConnections_;

  QMutex bufferMutex_;
  std::queue<std::unique_ptr<Data>> inBuffer_;
};
