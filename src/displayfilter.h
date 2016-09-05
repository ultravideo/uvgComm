#pragma once

#include "filter.h"

class VideoWidget;

class DisplayFilter : public Filter
{
public:
  DisplayFilter(StatisticsInterface* stats, VideoWidget *widget);

  void setProperties(bool mirror)
  {
    mirrored_ = mirror;
  }

protected:
  void process();

private:

  bool mirrored_;
  // Owned by VideoCall window
  VideoWidget* widget_;

};
