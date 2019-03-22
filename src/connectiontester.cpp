#include "connectiontester.h"
#include "stun.h"

ConnectionTester::ConnectionTester():
  rtp_pair_(nullptr),
  rtcp_pair_(nullptr),
  controller_(false)
{
}

ConnectionTester::~ConnectionTester()
{
}

void ConnectionTester::setCandidatePair(ICEPair *rtp_pair, ICEPair *rtcp_pair)
{
  rtp_pair_  = rtp_pair;
  rtcp_pair_ = rtcp_pair;
}

void ConnectionTester::isController(bool controller)
{
  controller_ = controller;
}

void ConnectionTester::run()
{
  if (rtp_pair_ == nullptr || rtcp_pair_ == nullptr)
  {
    qDebug() << "Unable to test connection, RTP or RTCP candidate is NULL!";
    return;
  }

  Stun stun;

  rtcp_pair_->state = PAIR_WAITING;
  rtp_pair_->state  = PAIR_IN_PROGRESS;

  if (stun.sendBindingRequest(rtp_pair_, true))
  {
    rtp_pair_->state = PAIR_SUCCEEDED;
    rtcp_pair_->state = PAIR_IN_PROGRESS;

    if (stun.sendBindingRequest(rtcp_pair_, true))
    {
      rtcp_pair_->state = PAIR_SUCCEEDED;
    }
    else
    {
      rtcp_pair_->state = PAIR_FAILED;
    }
  }
  else
  {
    rtp_pair_->state = PAIR_FAILED;
  }

  /* remote did not respond to our binding requests -> terminate */
  if (rtp_pair_->state == PAIR_FAILED || rtcp_pair_->state == PAIR_FAILED)
  {
    return;
  }

  // nomination is handled by the FlowController so if we're the controller,
  // terminate connection testing
  if (controller_)
  {
    emit testingDone(rtp_pair_, rtcp_pair_);
    return;
  }

  // otherwise continue sending requests to remote to keep the hole in firewall open
  if (!stun.sendNominationResponse(rtp_pair_))
  {
    qDebug() << "failed to receive nomination for RTP!";
    return;
  }

  if (!stun.sendNominationResponse(rtcp_pair_))
  {
    qDebug() << "failed to receive nomination for RTCP!";
    return;
  }

  emit testingDone(rtp_pair_, rtcp_pair_);
}
