#pragma once

#include "filter.h"

class VideoWidget;

class DisplayFilter : public Filter
{
public:
  DisplayFilter(QString id, StatisticsInterface* stats, VideoWidget *widget, uint32_t peer);
  ~DisplayFilter();
  void setProperties(bool mirror)
  {
    mirrored_ = mirror;
  }

protected:
  void process();

private:

  bool mirrored_;
  // Owned by Conference view
  VideoWidget* widget_;

  uint32_t peer_;

};
