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
      // Strict ordering: decoder does not accept out-of-order frames.
      Logger::getLogger()->printWarning(this, "Wrong order: dropping buffered timestamp older/equal to last output",
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

      if (switchingLinks_ && haveSecondarySsrc_ && out.back().ssrc == secondarySsrc_)
      {
        Logger::getLogger()->printWarning(this, "Link switch committed (started outputting new SSRC)",
                                         {"OldSSRC", "NewSSRC", "OutputTS"},
                                         {QString::number(primarySsrc_),
                                          QString::number(secondarySsrc_),
                                          QString::number(ts)});
        primarySsrc_ = secondarySsrc_;
        havePrimarySsrc_ = true;
        haveSecondarySsrc_ = false;
        switchingLinks_ = false;
        switchStartMs_ = -1;
      }
      continue;
    }

    // We still have a gap (timestamp jump too large).
    if (!waitingForMissing_)
    {
      waitingForMissing_ = true;
      if (!switchingLinks_)
        missingStartMs_ = nowMs;
    }
    break;
  }

  // If the gap persists long enough, assume the missing frame is lost and flush everything.
  if (waitingForMissing_ && !frameBuffer_.empty())
  {
    const int64_t startMs = switchingLinks_ ? switchStartMs_ : missingStartMs_;
    if (startMs <= 0)
      return out;

    const int64_t waited = nowMs - startMs;
    if (waited >= MAX_MISSING_WAIT_MS)
    {
      const uint32_t oldestBufferedTs = frameBuffer_.front().timestamp;
      const uint32_t newestBufferedTs = frameBuffer_.back().timestamp;
      const uint32_t bufferedFrames = static_cast<uint32_t>(frameBuffer_.size());
      const uint32_t deltaToOldest = static_cast<uint32_t>(oldestBufferedTs - lastTs);
      const uint32_t approxMissingFrames = (deltaToOldest / FIXED_FRAME_DELTA_30FPS);

      Logger::getLogger()->printWarning(this, "Missing frame timeout; skipping gap",
                                       {"WaitedMs", "LastOutputTS", "NextBufferedTS", "BufferedFrames", "NewestBufferedTS", "ApproxMissingFrames"},
                                       {QString::number(waited),
                                        QString::number(lastTs),
                                        QString::number(oldestBufferedTs),
                                        QString::number(bufferedFrames),
                                        QString::number(newestBufferedTs),
                                        QString::number(approxMissingFrames)});

      waitingForMissing_ = false;
      missingStartMs_ = -1;

      if (switchingLinks_ && haveSecondarySsrc_)
      {
        Logger::getLogger()->printWarning(this, "Link switch timeout; committing to new SSRC",
                                         {"OldSSRC", "NewSSRC"},
                                         {QString::number(primarySsrc_),
                                          QString::number(secondarySsrc_)});
        primarySsrc_ = secondarySsrc_;
        havePrimarySsrc_ = true;
        haveSecondarySsrc_ = false;
        switchingLinks_ = false;
        switchStartMs_ = -1;
      }

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

    if (input->ssrc != 0)
    {
      if (!havePrimarySsrc_)
      {
        primarySsrc_ = input->ssrc;
        havePrimarySsrc_ = true;
      }
      else if (input->ssrc != primarySsrc_)
      {
        if (!haveSecondarySsrc_)
        {
          secondarySsrc_ = input->ssrc;
          haveSecondarySsrc_ = true;
          switchingLinks_ = true;
          switchStartMs_ = nowMs;
          Logger::getLogger()->printWarning(this, "Detected SSRC switch (starting reorder window)",
                                           {"OldSSRC", "NewSSRC", "LastOutputTS", "FirstNewTS"},
                                           {QString::number(primarySsrc_),
                                            QString::number(secondarySsrc_),
                                            haveLastOutputTimestamp_ ? QString::number(lastOutputTimestamp_) : QString("-"),
                                            QString::number(input->rtpTimestamp)});
        }
        else if (input->ssrc != secondarySsrc_)
        {
          // Unexpected third SSRC; restart switch tracking to avoid mixing streams.
          Logger::getLogger()->printWarning(this, "Unexpected SSRC change; restarting switch tracking",
                                           {"PrimarySSRC", "SecondarySSRC", "NewSSRC"},
                                           {QString::number(primarySsrc_),
                                            QString::number(secondarySsrc_),
                                            QString::number(input->ssrc)});
          secondarySsrc_ = input->ssrc;
          haveSecondarySsrc_ = true;
          switchingLinks_ = true;
          switchStartMs_ = nowMs;
          frameBuffer_.clear();
          waitingForMissing_ = false;
          missingStartMs_ = -1;
          haveLastOutputTimestamp_ = false;
        }
      }
    }

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
            const uint32_t expectedNextTs = lastOutputTimestamp_ + FIXED_FRAME_DELTA_30FPS;
            Logger::getLogger()->printWarning(this, "Wrong order: non-contiguous timestamp; buffering",
                                             {"FrameTS", "LastOutputTS", "ExpectedNextTS", "DeltaFromLast"},
                                             {QString::number(input->rtpTimestamp),
                                              QString::number(lastOutputTimestamp_),
                                              QString::number(expectedNextTs),
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
      if (haveLastOutputTimestamp_)
      {
        const int32_t diffToLastOut = static_cast<int32_t>(f.timestamp - lastOutputTimestamp_);
        if (diffToLastOut > 0)
        {
          const uint32_t deltaFromLastOut = static_cast<uint32_t>(diffToLastOut);
          if (deltaFromLastOut > MAX_ACCEPTABLE_DELTA)
          {
            Logger::getLogger()->printWarning(this, "Assuming missing frames: output timestamp jump",
                                             {"PrevOutputTS", "NextOutputTS", "DeltaFromLast"},
                                             {QString::number(lastOutputTimestamp_),
                                              QString::number(f.timestamp),
                                              QString::number(deltaFromLastOut)});
          }
        }
      }

      setTimestamp(f.timestamp);
      for (auto& packet : f.nalUnits)
      {
        sendOutput(std::move(packet));
      }
    }

    input = getInput();
  }
}
