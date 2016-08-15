#pragma once

#include "filter.h"

#include "openHevcWrapper.h"



class OpenHEVCFilter : public Filter
{
public:
  OpenHEVCFilter();

  void init();

  virtual bool isInputFilter() const
  {
    return false;
  }

  virtual bool isOutputFilter() const
  {
    return false;
  }


protected:
  virtual void process();

private:
  OpenHevc_Handle handle_;
};
