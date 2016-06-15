#pragma once

#include <QPainter>

#include "filter.h"

class VideoWidget;

class DisplayFilter : public Filter
{
public:
  DisplayFilter(VideoWidget *widget);

  virtual bool isInputFilter() const
  {
    return false;
  }

  virtual bool isOutputFilter() const
  {
    return true;
  }
protected:
  void process();

private:
  // DO NOT FREE MEMORY HERE
  VideoWidget* widget_;

};
