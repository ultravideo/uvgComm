#pragma once

#include "flowagent.h"

class FlowControllee : public FlowAgent
{
  Q_OBJECT
public:
  FlowControllee();

protected:
    void run();
};
