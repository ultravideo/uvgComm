#pragma once

#include "global.h"
#include <QWaitCondition>
#include <QThread>
#include <QMutex>
#include <QRandomGenerator>

#ifndef _MSC_VER
#include <sys/time.h>
#else
#include <WinSock2.h>
#endif

#include <cstdint>
#include <vector>
#include <deque>
#include <memory>
#include <functional>
#include <chrono>

// One of the most fundamental classes of uvgComm. A filter is an indipendent data processing
// unit running on its own thread. Filters can be linked together to form a data processing pipeline
// called filter graph. A class inherited from filter can do any sort of processing to the data it
// receives. The filter sleeps when it does not have any input to process.

// the numbers will change as new formats are added, please use enum values
enum DataType {DT_NONE        = 0,
               DT_YUV420VIDEO = (1),
               DT_YUV422VIDEO = (1 << 1),

               DT_NV12VIDEO   = (1 << 2),
               DT_NV21VIDEO   = (1 << 3),

               DT_YUYVVIDEO   = (1 << 4), // also called YUY2
               DT_UYVYVIDEO   = (1 << 5),

               DT_ARGBVIDEO   = (1 << 7),
               DT_BGRAVIDEO   = (1 << 8), // reverse of ARGB
               DT_ABGRVIDEO   = (1 << 9), // reverse of RGBA
               DT_RGB32VIDEO  = (1 << 10), // also known as RGBA

               DT_RGB24VIDEO  = (1 << 11), // also known as RGBX
               DT_BGRXVIDEO   = (1 << 12), // also known as RAW, reverse of RGBX

               DT_MJPEGVIDEO  = (1 << 13), // jpeg compressed video, often used as intermediary compression
               DT_HEVCVIDEO   = (1 << 14),

               DT_RAWAUDIO    = (1 << 15),
               DT_OPUSAUDIO   = (1 << 16),

               DT_RTP         = (1 << 17)
};

enum DataSource {DS_UNKNOWN, DS_LOCAL, DS_REMOTE};

enum HEVC_NAL_UNIT_TYPE {TRAIL_R = 1, IDR_W_RADL = 19,
                         VPS_NUT = 32, SPS_NUT = 33, PPS_NUT = 34};

// RTP timestamp rates as specified in RFCs
// RFC 3551: video timestamp rate is 90000 Hz
// RFC 7587: Opus uses 48000 Hz
const int VIDEO_RTP_TIMESTAMP_RATE = 90000; // RTP timestamp rate in Hz
const int OPUS_RTP_TIMESTAMP_RATE = 48000;  // RTP timestamp rate in Hz

// Generate initial RTP timestamp using only lower 31 bits (RFC 3550)
// This prevents issues with signed/unsigned comparisons while maintaining randomness
uint32_t initializeRtpTimestamp();

// Update video RTP timestamp for next frame with proper rollover handling (RFC 3550)
// Calculates increment based on framerate and adds it to previous timestamp
// RTP timestamps are 32-bit and wrap around naturally
// frameCount: number of frames to advance (defaults to 1)
uint32_t updateVideoRtpTimestamp(uint32_t previousTimestamp, int framerateNumerator, int framerateDenominator, int frameCount = 1);

// Update audio RTP timestamp for next frame (RFC 3550)
// Uses OPUS_RTP_TIMESTAMP_RATE / AUDIO_FRAMES_PER_SECOND as increment
uint32_t updateAudioRtpTimestamp(uint32_t previousTimestamp);

QString datatypeToString(const DataType type);

struct VideoInfo
{
  int16_t width;
  int16_t height;
  int32_t framerateNumerator;
  int32_t framerateDenominator;

  bool flippedVertically = false;
  bool flippedHorizontally = false;

  // Indicates whether this video sample contains a keyframe (IDR/ intra)
  bool keyframe = false;

  RoiMap roi;
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

  // SSRC of the RTP stream that produced this packet (0 = unknown)
  uint32_t ssrc = 0;

  uint32_t rtpTimestamp = 0;

  // indicate the moment of creation for this sample for latency calculations
  int64_t creationTimestamp = -1;

  // indicate the intended presentation timestamp
  int64_t presentationTimestamp = -1;

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
  Data* shallowDataCopy(Data* original) const;
  Data* deepDataCopy(Data* original) const;

  QString getName() const
  {
    return name_;
  }

  // Returns the index of the out-connection that points to 'target'.
  // Returns -1 if not found.
  int getOutConnectionIndex(std::shared_ptr<Filter> target) const;

  // Enable or disable an out connection by index. Safe to call from other
  // threads; will lock the connection mutex briefly.
  void setOutConnectionEnabledByIndex(int index, bool enabled);

  static std::unique_ptr<Data> initializeData(DataType type, DataSource source);

  static bool isVideo(DataType type);
  static bool isAudio(DataType type);

protected:

  std::unique_ptr<Data> normalizeOrientation(std::unique_ptr<Data> video,
                                             bool forceHorizontalFlip = false);

  // return: oldest element in buffer, empty if none found
  std::unique_ptr<Data> getInput();

  //sends output to out connections
  void sendOutput(std::unique_ptr<Data> output, bool inverse = false);

  // QThread function that runs the processing
  void run();

  virtual void process() = 0;

  // TODO: Replace with returning NAL unit
  bool isHEVCIntra(const unsigned char *buff) const;
  bool isHEVCInter(const unsigned char *buff) const;

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

  StatisticsInterface* getStats() const
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

  void setOutputStatus(int index, bool status)
  {
    if (outConnections_.size() > index)
    {
      QMutexLocker lock(&connectionMutex_);
      outConnections_[index].enabled = status;
    }
  }

  int sizeOfOutputConnections() const
  {
    return outConnections_.size();
  }

  void printDataBytes(QString type, const uint8_t *payload, size_t size,
                      int bytes, int shift);

  unsigned int inputDiscarded_;
private:

  std::unique_ptr<Data> validityCheck(std::unique_ptr<Data> data, bool &ok);

  std::chrono::time_point<std::chrono::high_resolution_clock> getFrameTimepoint();
  void resetSynchronizationPoint(int32_t framerateNumerator,
                                 int32_t framerateDenominator);

  QString name_;
  QString id_;

  StatisticsInterface* stats_;
  QMutex *waitMutex_;
  QWaitCondition hasInput_;

  bool running_;

  std::vector<std::function<void(std::unique_ptr<Data>)> > outDataCallbacks_;

  QMutex connectionMutex_;
  struct FilterOutput
  {
    std::shared_ptr<Filter> filter;
    bool enabled;
  };

  std::vector<FilterOutput> outConnections_;

  QMutex bufferMutex_;
  //std::queue<std::unique_ptr<Data>> inBuffer_;
  std::deque<std::unique_ptr<Data>> inBuffer_;

  unsigned int inputTaken_;

  std::shared_ptr<ResourceAllocator> hwResources_;

  uint32_t filterID_;

  // optional smoothing of input frames to frame rate
  bool enforceFramerate_;
  std::chrono::time_point<std::chrono::high_resolution_clock> synchronizationPoint_;
  uint64_t framesSinceSynchronization_;
  int32_t framerateNumerator_;
  int32_t framerateDenominator_;
};
