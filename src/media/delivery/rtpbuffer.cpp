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
      targetFrame->nalUnits.push_back(std::move(input));
    }
    
    // Output oldest frame
    if (frameBuffer_.size() >= BUFFER_SIZE)
    {
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
