#pragma once
#include "filter.h"

#include <QImage>

class VideoInterface;

class DisplayFilter : public Filter
{
public:
  DisplayFilter(QString id, StatisticsInterface* stats,
                std::shared_ptr<ResourceAllocator> hwResources,
                QList<VideoInterface*> widgets, uint32_t peer);
  ~DisplayFilter();

  void setHorizontalMirroring(bool status)
  {
    horizontalMirroring_ = status;
  }

protected:
  void process();

private:

  /* The purpose of this function is to deliver frames to widgets
   * that draw the frames. The copy is needed since the final life
   * of the frames can be different. Non-copy operations
   * invalidate the data */
  std::unique_ptr<Data> deliverFrame(VideoInterface* screen,
                                     std::unique_ptr<Data> input,
                                     QImage::Format format,
                                     bool useCopy, bool mirrorHorizontally);

  bool horizontalMirroring_;

  // Owned by Conference view
  QList<VideoInterface*> widgets_;

  uint32_t sessionID_;
};
