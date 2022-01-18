#pragma once

#include <QMutex>

#include <deque>

// The goal of this class is to convert arbitary size frames to desired size
// audio frames

class AudioFrameBuffer
{
public:
  AudioFrameBuffer(uint32_t desiredFrameSize);
  ~AudioFrameBuffer();

  // Put any size data into frame buffer.
  // Does not modify or delete input data.
  void inputData(uint8_t* data, uint32_t dataAmount);

  // returns desired size frames if available. Returns nullptr if not
  // Gives away the ownership of the returned frame. Delete with delete[]
  uint8_t* readFrame();

  void changeDesiredFrameSize(uint32_t desiredSize);

  uint32_t getDesiredSize() const
  {
    return desiredFrameSize_;
  }

  size_t getBufferSize()
  {
    return readyFrames_.size();
  }

private:

  void addSampleToBuffer(uint8_t* sample, int sampleSize);

  uint32_t desiredFrameSize_;

  QMutex readyBuffer_;
  std::deque<uint8_t*> readyFrames_;

  // partialFrame_ memory is reserved at the beginning, released at the end
  // and not freed between inputs
  uint8_t* partialFrame_;

  // How much the partialFrame_ currently has data. Values should from 0 to
  // desiredFrameSize_.
  uint32_t partialFrameSize_;
};
