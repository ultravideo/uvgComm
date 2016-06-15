#ifndef RGB32TOYUV_H
#define RGB32TOYUV_H

#include "filter.h"


class RGB32toYUV : public Filter
{
public:
  RGB32toYUV();


  virtual bool isInputFilter() const
  {
    return false;
  }

  virtual bool isOutputFilter() const
  {
    return false;
  }

protected:
  void process();

};

#endif // RGB32TOYUV_H
