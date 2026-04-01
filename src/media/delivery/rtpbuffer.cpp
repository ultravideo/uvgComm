#include "rtpbuffer.h"
#include "logger.h"

constexpr uint32_t RTP_TIMESTAMP_WRAPAROUND = 0xFFFFFFFF; // 32-bit wraparound
constexpr uint32_t RTP_TIMESTAMP_RATE = 90000; // 1 second at 90kHz clock rate
constexpr uint32_t RTP_FRAME_INTERVAL = RTP_TIMESTAMP_RATE/30; // 1/30th of a second, assuming 30 fps video
constexpr uint32_t MAX_RTP_INTERVAL = RTP_FRAME_INTERVAL * 1.5;


RTPBuffer::RTPBuffer(QString id, StatisticsInterface* stats,
                     std::shared_ptr<ResourceAllocator> hwResources,
                     DataType type)
  : Filter(id, "RTPBuffer", stats, hwResources, type, type),
    currentSSRC_(0),
    currentRTPTimestamp_(0)
{}


void RTPBuffer::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    if (input->ssrc != currentSSRC_)
    {
      buffer_.push_back(std::move(input));

      if (currentRTPTimestamp_ != 0 && currentRTPTimestamp_ + MAX_RTP_INTERVAL < buffer_.front()->rtpTimestamp)
      {
        Logger::getLogger()->printNormal(this, "Buffering RTP packets due to SSRC change",
                                         "RTP Diff", QString::number(buffer_.front()->rtpTimestamp - currentRTPTimestamp_));
      }
      else
      {
        Logger::getLogger()->printNormal(this, "Change in SSRC resolved", 
          "Change", QString::number(currentSSRC_) + " -> " + QString::number(buffer_.front()->ssrc));
        currentSSRC_ = buffer_.front()->ssrc;
        emptyBuffer();
      }
    }
    else
    {
      currentRTPTimestamp_ = input->rtpTimestamp;
      sendOutput(std::move(input));
    }

    input = getInput();
  }
}


void RTPBuffer::emptyBuffer()
{
  Logger::getLogger()->printNormal(this, "Emptying buffer", {"Size", "Timestamp Diff"},
                                   {QString::number(buffer_.size()),
                                    QString::number(buffer_.front()->rtpTimestamp - currentRTPTimestamp_)});
  for (auto& nalUnit : buffer_)
  {
    currentRTPTimestamp_ = nalUnit->rtpTimestamp;
    sendOutput(std::move(nalUnit));
  }

  buffer_.clear();
}
