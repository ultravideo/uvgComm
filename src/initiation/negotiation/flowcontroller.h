#pragma once

#include "flowagent.h"

class FlowController : public FlowAgent
{
  Q_OBJECT
public:
  FlowController();

protected:
  virtual void nominationAction();

  int getTimeout()
  {
    return 10000;
  }

  bool isController()
  {
    return true;
  }
};
