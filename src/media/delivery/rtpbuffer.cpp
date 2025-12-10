#include "rtpbuffer.h"
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
    // Insert frame into reorder buffer maintaining RTP timestamp order
    // Use RFC 3550 signed difference to handle timestamp wraparound
    auto insertPos = reorderBuffer_.begin();
    while (insertPos != reorderBuffer_.end())
    {
      // Compare using signed difference to handle wraparound correctly
      int32_t diff = (int32_t)(input->rtpTimestamp - (*insertPos)->rtpTimestamp);
      if (diff < 0)
      {
        break; // Found insertion point
      }
      ++insertPos;
    }
    reorderBuffer_.insert(insertPos, std::move(input));

    // Output oldest frame once buffer reaches target size
    if (reorderBuffer_.size() >= BUFFER_SIZE)
    {
      sendOutput(std::move(reorderBuffer_.front()));
      reorderBuffer_.pop_front();
    }

    input = getInput();
  }
}
