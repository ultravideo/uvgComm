#pragma once
#include "filter.h"

class VideoInterface;

class DisplayFilter : public Filter
{
public:
  DisplayFilter(QString id, StatisticsInterface* stats, VideoInterface *widget, uint32_t peer);
  ~DisplayFilter();

  void updateSettings();

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
  bool flipEnabled_;

  // Owned by Conference view
  VideoInterface* widget_;

  uint32_t peer_;
};
