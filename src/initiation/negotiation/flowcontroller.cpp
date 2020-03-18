#include "flowcontroller.h"

#include "connectiontester.h"
#include "common.h"

FlowController::FlowController()
{}

void FlowController::nominationAction()
{
  Stun stun;
  if (!stun.sendNominationRequest(nominated_rtp_.get()))
  {
    printDebug(DEBUG_ERROR, "FlowAgent",  "Failed to nominate RTP candidate!");
    emit ready(nullptr, nullptr, sessionID_);
    return;
  }

  if (!stun.sendNominationRequest(nominated_rtcp_.get()))
  {
    printDebug(DEBUG_ERROR, "FlowAgent",  "Failed to nominate RTCP candidate!");
    emit ready(nominated_rtp_, nullptr, sessionID_);
    return;
  }

  nominated_rtp_->state  = PAIR_NOMINATED;
  nominated_rtcp_->state = PAIR_NOMINATED;

  // media transmission can be started
  emit ready(nominated_rtp_, nominated_rtcp_, sessionID_);
}
