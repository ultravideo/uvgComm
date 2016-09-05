#pragma once

#include "filter.h"

class VideoWidget;

class DisplayFilter : public Filter
{
public:
  DisplayFilter(StatisticsInterface* stats, VideoWidget *widget);

  virtual bool isInputFilter() const
  {
    return false;
  }

  virtual bool isOutputFilter() const
  {
    return true;
  }

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
