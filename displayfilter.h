#pragma once

#include <QPainter>

#include "filter.h"
#include "videowidget.h"



class DisplayFilter : public Filter
{
public:
  DisplayFilter();


  void paint(QPainter *painter);

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
  VideoWidget vw_;

};
