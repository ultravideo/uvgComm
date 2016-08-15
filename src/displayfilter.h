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

  void setProperties(bool mirror, QSize scale)
  {
    mirrored_ = mirror;
    scale_ = scale;
  }

protected:
  void process();

private:

  bool mirrored_;
  QSize scale_;
  // DO NOT FREE MEMORY HERE
  VideoWidget* widget_;

};
