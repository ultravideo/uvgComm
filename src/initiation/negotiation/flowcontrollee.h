#pragma once

#include "flowagent.h"

class FlowControllee : public FlowAgent
{
  Q_OBJECT
public:
  FlowControllee();

protected:
  virtual void nominationAction();

  virtual int getTimeout()
  {
    return 20000;
  }

  bool isController()
  {
    return false;
  }
};
