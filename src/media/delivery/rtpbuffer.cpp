#include "rtpbuffer.h"
#include "logger.h"

#include <cstring>

constexpr uint32_t RTP_TIMESTAMP_WRAPAROUND = 0xFFFFFFFF; // 32-bit wraparound
constexpr uint32_t RTP_TIMESTAMP_HALF_RANGE = 0x80000000; // half of 32-bit space
constexpr uint32_t RTP_TIMESTAMP_RATE = 90000; // 1 second at 90kHz clock rate
constexpr uint32_t RTP_FRAME_INTERVAL = RTP_TIMESTAMP_RATE/30; // 1/30th of a second, assuming 30 fps video
constexpr uint32_t MAX_RTP_INTERVAL = RTP_FRAME_INTERVAL * 1.5;

namespace
{
// Signature used by HybridFilter::sendDummies(). These packets exist only to
// provoke RTCP behavior on otherwise-inactive paths; they should not affect
// receiver-side RTP buffering / SSRC change handling.
constexpr uint8_t kHybridDummyPacket[] = {
    0x00, 0x00, 0x00, 0x01,
    0x4E,
    0x7B, 0x01,
    0x00,
    0x80
};

bool isHybridDummyPacket(const Data* d)
{
  if (!d || !d->data)
    return false;
  if (d->data_size != sizeof(kHybridDummyPacket))
    return false;

  return std::memcmp(d->data.get(), kHybridDummyPacket, sizeof(kHybridDummyPacket)) == 0;
}

inline bool isRtpTimestampNewer(uint32_t current, uint32_t candidate)
{
  const uint32_t delta = candidate - current;

  // RFC3550-style serial arithmetic: candidate is newer if it's ahead by
  // less than half the sequence space (and not equal).
  return delta != 0 && delta < RTP_TIMESTAMP_HALF_RANGE;
}

inline uint32_t rtpTimestampDiffForward(uint32_t from, uint32_t to)
{
  // to has not wrapped around
  if (to >= from)
    return to - from;

  // to has wrapped around, use wrap around comparison
  return (RTP_TIMESTAMP_WRAPAROUND - from) + to + 1;
}
}


RTPBuffer::RTPBuffer(QString id, StatisticsInterface* stats,
                     std::shared_ptr<ResourceAllocator> hwResources,
                     DataType type)
  : Filter(id, "RTPBuffer", stats, hwResources, type, type),
  currentSSRC_(0),
  currentRTPTimestamp_(0),
  timestampInitialized_(false)
{}


void RTPBuffer::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    if (isHybridDummyPacket(input.get()))
    {
      Logger::getLogger()->printNormal(this, "Dropped Hybrid dummy packet in RTPBuffer", "SSRC", QString::number(input->ssrc));
      input = getInput();
      continue;
    }

    if (input->ssrc != currentSSRC_) // buffer new link packets during switch
    { 
      if (buffer_.empty())
      {
        Logger::getLogger()->printNormal(this, "Detected new SSRC, starting synchronization", 
          "Change", QString::number(currentSSRC_) + " -> " + QString::number(input->ssrc));
      }

      buffer_.push_back(std::move(input));

      if (timestampInitialized_ && !isRtpTimestampNewer(currentRTPTimestamp_, buffer_.back()->rtpTimestamp))
      {
        Logger::getLogger()->printWarning(this, "Discarding stale buffered packet during SSRC transition",
                                          {"CurrentTS", "IncomingTS"},
                                          {QString::number(currentRTPTimestamp_), QString::number(buffer_.front()->rtpTimestamp)});
        buffer_.pop_back();
      }
      else if (timestampInitialized_ && rtpTimestampDiffForward(currentRTPTimestamp_, buffer_.front()->rtpTimestamp) > MAX_RTP_INTERVAL)
      {
        Logger::getLogger()->printNormal(this, "Buffering RTP packets due to SSRC change",
                                         "RTP Diff", QString::number(rtpTimestampDiffForward(currentRTPTimestamp_, buffer_.front()->rtpTimestamp)));
      }
      else
      {
        Logger::getLogger()->printNormal(this, "Change in SSRC resolved", 
          "Change", QString::number(currentSSRC_) + " -> " + QString::number(buffer_.front()->ssrc));
        currentSSRC_ = buffer_.front()->ssrc;
        emptyBuffer();
      }
    }
    else // normal operation
    {
      currentRTPTimestamp_ = input->rtpTimestamp;
      timestampInitialized_ = true;
      sendOutput(std::move(input));
    }

    input = getInput();
  }
}


void RTPBuffer::emptyBuffer()
{
  Logger::getLogger()->printNormal(this, "Emptying buffer", {"Size", "Timestamp Diff"},
                                   {QString::number(buffer_.size()),
                                    QString::number((int64_t)(buffer_.front()->rtpTimestamp - currentRTPTimestamp_))});
  for (auto& nalUnit : buffer_)
  {
    currentRTPTimestamp_ = nalUnit->rtpTimestamp;
    timestampInitialized_ = true;
    sendOutput(std::move(nalUnit));
  }

  buffer_.clear();
}
