#pragma once
#include "filter.h"

class VideoWidget;

class DisplayFilter : public Filter
{
public:
  DisplayFilter(QString id, StatisticsInterface* stats, VideoWidget *widget, uint32_t peer);
  ~DisplayFilter();
  void setProperties(bool mirrorHorizontal, bool mirrorVertical)
  {
    horizontalMirroring_ = mirrorHorizontal;
    verticalMirroring_ = mirrorVertical;
  }

protected:
  void process();

private:

  bool horizontalMirroring_;
  bool verticalMirroring_;

  // Owned by Conference view
  VideoWidget* widget_;

  uint32_t peer_;

};
