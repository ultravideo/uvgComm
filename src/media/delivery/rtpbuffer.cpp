#include "rtpbuffer.h"
#include "logger.h"
#include <algorithm>

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
    bool processed = false;
    for (auto insertPos = reorderBuffer_.begin(); insertPos != reorderBuffer_.end(); ++insertPos)
    {
      int32_t diff = (int32_t)(input->rtpTimestamp - (*insertPos)->rtpTimestamp);
      
      if (diff == 0 && input->data_size == (*insertPos)->data_size)
      {
        Logger::getLogger()->printWarning(this, "Duplicate RTP packet detected and discarded",
                                         {"RTP Timestamp", "Size"},
                                         {QString::number(input->rtpTimestamp), 
                                          QString::number(input->data_size)});
        processed = true; // discard duplicate
        break;
      }
      
      if (diff < 0) // earlier packet than buffered one
      {
        Logger::getLogger()->printWarning(this, "Out-of-order RTP packet reordered",
                                         {"RTP Timestamp", "Expected after"},
                                         {QString::number(input->rtpTimestamp), 
                                          QString::number((*insertPos)->rtpTimestamp)});
        reorderBuffer_.insert(insertPos, std::move(input));
        processed = true;
        break;
      }
    }
    
    if (!processed) // newest packet to end
    {
      reorderBuffer_.push_back(std::move(input));
    }
    
    // Output oldest timestamp packets when buffer is full
    // Don't output if first and last packet have same timestamp (incomplete frame)
    if (reorderBuffer_.size() >= BUFFER_SIZE &&
        reorderBuffer_.front()->rtpTimestamp != reorderBuffer_.back()->rtpTimestamp)
    {
      uint32_t oldestTimestamp = reorderBuffer_.front()->rtpTimestamp;
      
      while (!reorderBuffer_.empty() && 
             reorderBuffer_.front()->rtpTimestamp == oldestTimestamp)
      {
        sendOutput(std::move(reorderBuffer_.front()));
        reorderBuffer_.pop_front();
      }
    }

    input = getInput();
  }
}
