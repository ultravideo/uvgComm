#include "rtpbuffer.h"
#include "logger.h"

RTPBuffer::RTPBuffer(QString id, StatisticsInterface* stats,
                     std::shared_ptr<ResourceAllocator> hwResources,
                     DataType type)
  : Filter(id, "RTPBuffer", stats, hwResources, type, type)
{}

void RTPBuffer::process()
{
  std::unique_ptr<Data> input = getInput();

  while (input)
  { 
    // Find or create frame for this timestamp
    Frame* targetFrame = nullptr;
    bool isDuplicate = false;
    
    for (auto it = frameBuffer_.begin(); it != frameBuffer_.end(); ++it)
    {
      int32_t diff = (int32_t)(input->rtpTimestamp - it->timestamp);
      
      if (diff == 0) // we have these timestamps already
      {
        for (const auto& packet : it->nalUnits)
        {
          if (packet->data_size == input->data_size) // duplicate
          {
            Logger::getLogger()->printWarning(this, "Duplicate RTP packet detected and discarded",
                                             {"RTP Timestamp", "Size"},
                                             {QString::number(input->rtpTimestamp),
                                              QString::number(input->data_size)});
            isDuplicate = true;
            break;
          }
        }
        targetFrame = &(*it);
        break;
      }
      else if (diff < 0) // wrong timestamp order
      {
        // Earlier timestamp - insert new frame before this one
        Logger::getLogger()->printWarning(this, "Out-of-order frame reordered",
                                         {"RTP Timestamp", "Expected after"},
                                         {QString::number(input->rtpTimestamp),
                                          QString::number(it->timestamp)});
        targetFrame = &(*frameBuffer_.insert(it, {input->rtpTimestamp, {}}));
        break;
      }
    }
    
    // If not found, add new frame at end
    if (!targetFrame)
    {
      frameBuffer_.push_back({input->rtpTimestamp, {}});
      targetFrame = &frameBuffer_.back();
    }
    
    if (!isDuplicate) // multiple NAL units with same timestamp
    {
      // assign SSRC to the frame if unknown
      if (targetFrame->ssrc == 0 && input->ssrc != 0)
      {
        targetFrame->ssrc = input->ssrc;
      }
      else if (targetFrame->ssrc != 0 && input->ssrc != 0 && targetFrame->ssrc != input->ssrc)
      {
        Logger::getLogger()->printWarning(this, "Same RTP timestamp contains packets from different SSRCs",
                                         {"RTP Timestamp", "Frame SSRC", "Packet SSRC"},
                                         {QString::number(targetFrame->timestamp),
                                          QString::number(targetFrame->ssrc),
                                          QString::number(input->ssrc)});
      }

      targetFrame->nalUnits.push_back(std::move(input));
    }
    
    // Detect SSRC change and enter hold-mode if needed
    if (!holdMode_)
    {
      // initialize current output SSRC if unknown
      if (currentOutputSSRC_ == 0 && !frameBuffer_.empty() && frameBuffer_.front().ssrc != 0)
      {
        currentOutputSSRC_ = frameBuffer_.front().ssrc;
      }

      // If the newly inserted/updated frame has a different SSRC than current output, start hold-mode
      if (targetFrame->ssrc != 0 && currentOutputSSRC_ != 0 && targetFrame->ssrc != currentOutputSSRC_)
      {
        pendingNewSSRC_ = targetFrame->ssrc;
        pendingSwitchTimestamp_ = targetFrame->timestamp;

        // determine highest timestamp we have seen for the old SSRC in buffer
        pendingOldMaxTimestamp_ = 0;
        for (const auto &f : frameBuffer_)
        {
          if (f.ssrc == currentOutputSSRC_ && f.timestamp > pendingOldMaxTimestamp_)
            pendingOldMaxTimestamp_ = f.timestamp;
        }

        // Compute threshold: we consider it safe to switch immediately if old max TS
        // has reached new TS - 1.5 * interval
        int64_t offset = (int64_t)estimatedFrameDelta_ * 3 / 2; // 1.5 * interval
        int64_t threshold = (int64_t)pendingSwitchTimestamp_ - offset;

        if ((int64_t)pendingOldMaxTimestamp_ >= threshold)
        {
          // No need to hold — flush existing buffer and switch immediately
          Logger::getLogger()->printNormal(this, "RTPBuffer immediate switch (no hold) due to old-stream progress",
                                           {"Current SSRC", "New SSRC", "Switch RTP Timestamp", "OldMaxTS", "Threshold"},
                                           {QString::number(currentOutputSSRC_),
                                            QString::number(pendingNewSSRC_),
                                            QString::number(pendingSwitchTimestamp_),
                                            QString::number(pendingOldMaxTimestamp_),
                                            QString::number(threshold)});

          // Flush buffered frames now in timestamp order, leaving BUFFER_SIZE frames buffered
          while (frameBuffer_.size() > BUFFER_SIZE)
          {
            Frame& f = frameBuffer_.front();
            for (auto& packet : f.nalUnits)
            {
              sendOutput(std::move(packet));
            }
            frameBuffer_.pop_front();
          }

          currentOutputSSRC_ = pendingNewSSRC_;
          pendingNewSSRC_ = 0;
          pendingSwitchTimestamp_ = 0;
          pendingOldMaxTimestamp_ = 0;
          holdMode_ = false;
        }
        else
        {
          // Otherwise enter hold-mode until old source reaches threshold
          holdMode_ = true;
          Logger::getLogger()->printNormal(this, "Entering RTPBuffer hold-mode due to SSRC change",
                                           {"Current SSRC", "New SSRC", "Switch RTP Timestamp", "OldMaxTS", "Threshold"},
                                           {QString::number(currentOutputSSRC_),
                                            QString::number(pendingNewSSRC_),
                                            QString::number(pendingSwitchTimestamp_),
                                            QString::number(pendingOldMaxTimestamp_),
                                            QString::number(threshold)});
        }
      }
    }
    else
    {
      // update max timestamp seen for the old SSRC while we are holding
      if (targetFrame->ssrc == currentOutputSSRC_ && targetFrame->timestamp > pendingOldMaxTimestamp_)
      {
        pendingOldMaxTimestamp_ = targetFrame->timestamp;
      }

      // check if we can exit hold-mode: old source has progressed close enough to new stream
      int64_t offset = (int64_t)estimatedFrameDelta_ * 3 / 2; // 1.5 * interval
      int64_t threshold = (int64_t)pendingSwitchTimestamp_ - offset;
      if ((int64_t)pendingOldMaxTimestamp_ >= threshold)
      {
        Logger::getLogger()->printNormal(this, "RTPBuffer exiting hold-mode; flushing buffered frames",
                                         {"PendingNewSSRC", "OldMaxTS", "Threshold"},
                                         {QString::number(pendingNewSSRC_),
                                          QString::number(pendingOldMaxTimestamp_),
                                          QString::number(threshold)});

        // Flush buffered frames in timestamp order, leaving BUFFER_SIZE frames buffered
        while (frameBuffer_.size() > BUFFER_SIZE)
        {
          Frame& f = frameBuffer_.front();
          for (auto& packet : f.nalUnits)
          {
            sendOutput(std::move(packet));
          }
          frameBuffer_.pop_front();
        }

        // switch current output SSRC to new
        currentOutputSSRC_ = pendingNewSSRC_;
        pendingNewSSRC_ = 0;
        pendingSwitchTimestamp_ = 0;
        pendingOldMaxTimestamp_ = 0;
        holdMode_ = false;
      }
    }

    // Output oldest frames (keep BUFFER_SIZE frames buffered).
    // In hold-mode, only output frames from the current (old) SSRC.
    while (frameBuffer_.size() > BUFFER_SIZE)
    {
      if (holdMode_)
      {
        if (currentOutputSSRC_ == 0 || frameBuffer_.front().ssrc != currentOutputSSRC_)
        {
          break;
        }
      }

      Frame& oldestFrame = frameBuffer_.front();
      for (auto& packet : oldestFrame.nalUnits)
      {
        sendOutput(std::move(packet));
      }
      frameBuffer_.pop_front();
    }

    input = getInput();
  }
}
