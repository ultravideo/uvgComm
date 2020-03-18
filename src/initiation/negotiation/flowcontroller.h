#pragma once

#include "flowagent.h"

class FlowController : public FlowAgent
{
  Q_OBJECT
public:
  FlowController();

protected:
    void run();
};
