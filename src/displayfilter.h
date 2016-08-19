#pragma once

#include <QPainter>
#include <QSize>

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

  void setProperties(bool mirror)
  {
    mirrored_ = mirror;
  }

protected:
  void process();

private:

  bool mirrored_;
  // DO NOT FREE MEMORY HERE
  VideoWidget* widget_;

};
