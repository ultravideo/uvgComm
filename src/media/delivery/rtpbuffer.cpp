#include "rtpbuffer.h"
#include "logger.h"

#include "common.h"

RTPBuffer::RTPBuffer(QString id, StatisticsInterface* stats,
                     std::shared_ptr<ResourceAllocator> hwResources,
                     DataType type)
  : Filter(id, "RTPBuffer", stats, hwResources, type, type)
{}

void RTPBuffer::bufferFrame(std::unique_ptr<Data> packet)
{
  if (!packet)
    return;

  if (!frameBuffer_.empty())
  {
    const int32_t diffToNewestBuffered =
        static_cast<int32_t>(packet->rtpTimestamp - frameBuffer_.back().timestamp);
    if (diffToNewestBuffered < 0)
    {
      Logger::getLogger()->printWarning(this, "Wrong order: buffered timestamp older than newest buffered",
                                       {"FrameTS", "NewestBufferedTS"},
                                       {QString::number(packet->rtpTimestamp),
                                        QString::number(frameBuffer_.back().timestamp)});
    }
  }

  Frame* target = nullptr;
  for (auto it = frameBuffer_.begin(); it != frameBuffer_.end(); ++it)
  {
    const int32_t diff = static_cast<int32_t>(packet->rtpTimestamp - it->timestamp);
    if (diff == 0)
    {
      target = &(*it);
      break;
    }
    if (diff < 0)
    {
      target = &(*frameBuffer_.insert(it, Frame{packet->rtpTimestamp, packet->ssrc, {}}));
      break;
    }
  }

  if (!target)
  {
    frameBuffer_.push_back(Frame{packet->rtpTimestamp, packet->ssrc, {}});
    target = &frameBuffer_.back();
  }

  if (target->ssrc == 0 && packet->ssrc != 0)
    target->ssrc = packet->ssrc;

  target->nalUnits.push_back(std::move(packet));
}

std::vector<Frame> RTPBuffer::collectFramesToOutput(int64_t nowMs)
{
  std::vector<Frame> out;

  if (!haveLastOutputTimestamp_)
    return out;

  uint32_t lastTs = lastOutputTimestamp_;

  // Flush any contiguous frames we already have.
  while (!frameBuffer_.empty())
  {
    const uint32_t ts = frameBuffer_.front().timestamp;

    const int32_t diffToLast = static_cast<int32_t>(ts - lastTs);
    if (diffToLast <= 0)
    {
      Logger::getLogger()->printWarning(this, "Dropping late frame older than expected",
                                       {"FrameTS", "LastOutputTS"},
                                       {QString::number(ts), QString::number(lastTs)});
      frameBuffer_.pop_front();
      continue;
    }

    const uint32_t deltaFromLast = static_cast<uint32_t>(diffToLast);
    if (deltaFromLast >= MIN_ACCEPTABLE_DELTA && deltaFromLast <= MAX_ACCEPTABLE_DELTA)
    {
      // Gap resolved (we have an acceptable next timestamp), so exit waiting mode.
      waitingForMissing_ = false;
      missingStartMs_ = -1;

      out.emplace_back(std::move(frameBuffer_.front()));
      frameBuffer_.pop_front();

      lastTs = ts;
      continue;
    }

    // We still have a gap (timestamp jump too large).
    if (!waitingForMissing_)
    {
      waitingForMissing_ = true;
      missingStartMs_ = nowMs;
    }
    break;
  }

  // If the gap persists long enough, assume the missing frame is lost and flush everything.
  if (waitingForMissing_ && !frameBuffer_.empty() && missingStartMs_ > 0)
  {
    const int64_t waited = nowMs - missingStartMs_;
    if (waited >= MAX_MISSING_WAIT_MS)
    {
      Logger::getLogger()->printWarning(this, "Missing frame timeout; skipping gap",
                                       {"WaitedMs", "LastOutputTS", "NextBufferedTS"},
                                       {QString::number(waited),
                                        QString::number(lastTs),
                                        QString::number(frameBuffer_.front().timestamp)});

      waitingForMissing_ = false;
      missingStartMs_ = -1;

      while (!frameBuffer_.empty())
      {
        out.emplace_back(std::move(frameBuffer_.front()));
        frameBuffer_.pop_front();
      }
    }
  }

  return out;
}

void RTPBuffer::setTimestamp(uint32_t outputRtpTimestamp)
{
  lastOutputTimestamp_ = outputRtpTimestamp;
  haveLastOutputTimestamp_ = true;
}

void RTPBuffer::process()
{
  // Drain currently queued input quickly.
  std::unique_ptr<Data> input = getInput();
  while (input)
  {
    const int64_t nowMs = clockNowMs();

    // Always forward additional NAL units that share the last output timestamp.
    // This handles cases like SPS/PPS arriving with the same timestamp as the last NAL unit.
    if (haveLastOutputTimestamp_ && input->rtpTimestamp == lastOutputTimestamp_)
    {
      sendOutput(std::move(input));
      input = getInput();
      continue;
    }

    if (!waitingForMissing_)
    {
      if (!haveLastOutputTimestamp_)
      {
        setTimestamp(input->rtpTimestamp);
        sendOutput(std::move(input));
      }
      else
      {
        const int32_t diffToLast =
            static_cast<int32_t>(input->rtpTimestamp - lastOutputTimestamp_);

        if (diffToLast > 0)
        {
          const uint32_t deltaFromLast = static_cast<uint32_t>(diffToLast);
          if (deltaFromLast >= MIN_ACCEPTABLE_DELTA && deltaFromLast <= MAX_ACCEPTABLE_DELTA)
          {
            // Accept next timestamp with tolerance and use it as-is.
            setTimestamp(input->rtpTimestamp);
            sendOutput(std::move(input));
          }
          else
          {
            Logger::getLogger()->printWarning(this, "Wrong order: non-contiguous timestamp; buffering",
                                             {"FrameTS", "LastOutputTS", "DeltaFromLast"},
                                             {QString::number(input->rtpTimestamp),
                                              QString::number(lastOutputTimestamp_),
                                              QString::number(deltaFromLast)});
            waitingForMissing_ = true;
            missingStartMs_ = nowMs;
            bufferFrame(std::move(input));
          }
        }
        else
        {
          Logger::getLogger()->printWarning(this, "Wrong order: dropping late timestamp older than last output",
                                           {"FrameTS", "LastOutputTS"},
                                           {QString::number(input->rtpTimestamp),
                                            QString::number(lastOutputTimestamp_)});
        }
      }
    }
    else
    {
      bufferFrame(std::move(input));
    }

    std::vector<Frame> readyFrames = collectFramesToOutput(nowMs);
    for (auto& f : readyFrames)
    {
      setTimestamp(f.timestamp);
      for (auto& packet : f.nalUnits)
      {
        sendOutput(std::move(packet));
      }
    }

    input = getInput();
  }
}
