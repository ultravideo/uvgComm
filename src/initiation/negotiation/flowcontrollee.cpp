#include "flowcontrollee.h"

#include "connectiontester.h"
#include "common.h"

FlowControllee::FlowControllee()
{}


void FlowControllee::nominationAction()
{
  // media transmission can be started
  emit ready(nominated_rtp_, nominated_rtcp_, sessionID_);
}
