#include "iceflowcontrol.h"
#include "ice.h"

FlowAgent::FlowAgent():
  candidates_(nullptr),
  sessionID_(0)
{
}

FlowAgent::~FlowAgent()
{
  delete candidates_;
}

void FlowAgent::run() { }

void FlowAgent::setCandidates(QList<ICEPair *> *candidates)
{
  candidates_ = candidates;
}

void FlowAgent::setSessionID(uint32_t sessionID)
{
  sessionID_ = sessionID;
}

void FlowController::run()
{
  Stun stun;
  QList<ICEPair *> validPairs;
  ICEPair *cand_rtp = nullptr;
  ICEPair *cand_rtcp = nullptr;

  if (candidates_ == nullptr || candidates_->size() == 0)
  {
    qDebug() << "ERROR: invalid candidates, unable to perform ICE candidate negotiation!";
    emit ready(nullptr, nullptr, sessionID_);
    return;
  }

  for (int i = 0; i < candidates_->size(); i += 2)
  {
    // RTP
    if (stun.sendBindingRequest(candidates_->at(i)->remote->address, candidates_->at(i)->remote->port,
                                candidates_->at(i)->local->address,  candidates_->at(i)->local->port, true))
    {

      // RTCP
      if (stun.sendBindingRequest(candidates_->at(i + 1)->remote->address, candidates_->at(i + 1)->remote->port,
                                  candidates_->at(i + 1)->local->address,  candidates_->at(i + 1)->local->port, true))
      {
        candidates_->at(i + 0)->state = PAIR_SUCCEEDED;
        candidates_->at(i + 1)->state = PAIR_SUCCEEDED;

        // both RTP and RTCP must succeeed in order for this pair to be considered valid
        validPairs.push_back(candidates_->at(i + 0));
        validPairs.push_back(candidates_->at(i + 1));

        cand_rtp  = candidates_->at(i + 0);
        cand_rtcp = candidates_->at(i + 1);
        break;
      }
      else
      {
        qDebug() << "[controller] rtcp failed";
      }
    }
    else
    {
      qDebug() << "[controller] rtp failed!";
    }

    candidates_->at(i + 0)->state = PAIR_FAILED;
    candidates_->at(i + 1)->state = PAIR_FAILED;
  }

  if (validPairs.size() < 2)
  {
    qDebug() << "ERROR: Failed to negotiate a list of valid candidates with remote!";
    goto end;
  }

  /* qDebug() << "[controller] rtp valid pair:" << cand_rtp->local->address << ":" << cand_rtp->local->port; */
  /* qDebug() << "[controller] rtp valid pair:" << cand_rtp->remote->address << ":" << cand_rtp->remote->port; */
  /* qDebug() << "[controller] rtcp valid pair:" << cand_rtcp->local->address << ":" << cand_rtcp->local->port; */
  /* qDebug() << "[controller] rtcp valid pair:" << cand_rtcp->remote->address << ":" << cand_rtcp->remote->port; */
  /* qDebug() << "[controller] STARTING NOMINATION!"; */

  // nominate RTP candidate
  if (!stun.sendNominationRequest(validPairs[0]->remote->address, validPairs[0]->remote->port,
                                  validPairs[0]->local->address,  validPairs[0]->local->port))
  {
    qDebug() << "[controller] RTP CANDIDATE FAILED";
    cand_rtp = nullptr;
    goto end;
  }

  // nominate RTCP candidate
  if (!stun.sendNominationRequest(validPairs[1]->remote->address, validPairs[1]->remote->port,
                                  validPairs[1]->local->address,  validPairs[1]->local->port))
  {
    qDebug() << "[controller] RTCP CANDIDATE FAILED";
    cand_rtcp = nullptr;
  }

end:
  // release all failed/frozen candidates
  for (int i = 0; i < candidates_->size(); ++i)
  {
    if (candidates_->at(i)->state != PAIR_SUCCEEDED)
    {
      delete candidates_->at(i);
    }
  }

  emit ready(cand_rtp, cand_rtcp, sessionID_);
}

void FlowControllee::run()
{
  Stun stun;
  QList<ICEPair *> validPairs;
  ICEPair *cand_rtp = nullptr;
  ICEPair *cand_rtcp = nullptr;

  if (candidates_ == nullptr || candidates_->size() == 0)
  {
    qDebug() << "ERROR: invalid candidates, unable to perform ICE candidate negotiation!";
    emit ready(nullptr, nullptr, sessionID_);
    return;
  }

  for (int i = 0; i < candidates_->size(); i += 2)
  {
    // RTP
    if (stun.sendBindingRequest(candidates_->at(i)->remote->address, candidates_->at(i)->remote->port,
                                candidates_->at(i)->local->address,  candidates_->at(i)->local->port, true))
    {

      // RTCP
      if (stun.sendBindingRequest(candidates_->at(i + 1)->remote->address, candidates_->at(i + 1)->remote->port,
                                  candidates_->at(i + 1)->local->address,  candidates_->at(i + 1)->local->port, true))
      {
        candidates_->at(i + 0)->state = PAIR_SUCCEEDED;
        candidates_->at(i + 1)->state = PAIR_SUCCEEDED;

        // both RTP and RTCP must succeeed in order for this pair to be considered valid
        validPairs.push_back(candidates_->at(i + 0));
        validPairs.push_back(candidates_->at(i + 1));

        cand_rtp  = candidates_->at(i + 0);
        cand_rtcp = candidates_->at(i + 1);
        break;
      }
      else
      {
        qDebug() << "[controllee] rtcp failed!";
      }
    }
    else
    {
      qDebug() << "[controllee] rtp failed!";
    }

    candidates_->at(i + 0)->state = PAIR_FAILED;
    candidates_->at(i + 1)->state = PAIR_FAILED;
  }

  if (validPairs.size() < 2)
  {
    qDebug() << "ERROR: Failed to negotiate a list of valid candidates with remote!";
    goto end;
  }

  /* qDebug() << "[controllee] rtp valid pair:" << cand_rtp->local->address << ":" << cand_rtp->local->port; */
  /* qDebug() << "[controllee] rtp valid pair:" << cand_rtp->remote->address << ":" << cand_rtp->remote->port; */
  /* qDebug() << "[controllee] rtcp valid pair:" << cand_rtcp->local->address << ":" << cand_rtcp->local->port; */
  /* qDebug() << "[controllee] rtcp valid pair:" << cand_rtcp->remote->address << ":" << cand_rtcp->remote->port; */
  /* qDebug() << "[controllee] RESPONDING TO NOMINATIONS!"; */

  // respond to RTP nomination
  if (!stun.sendNominationResponse(validPairs[0]->remote->address, validPairs[0]->remote->port,
                                   validPairs[0]->local->address,  validPairs[0]->local->port))
  {
    qDebug() << "ERROR: RTP candidate nomination failed!";
    cand_rtp = nullptr;
    goto end;
  }

  // respond to RTCP nomination
  if (!stun.sendNominationResponse(validPairs[1]->remote->address, validPairs[1]->remote->port,
                                   validPairs[1]->local->address,  validPairs[1]->local->port))
  {
    qDebug() << "ERROR: RTCP candidate nomination failed!";
    cand_rtcp = nullptr;
    goto end;
  }


end:
  // release all failed/frozen candidates
  for (int i = 0; i < candidates_->size(); ++i)
  {
    if (candidates_->at(i)->state != PAIR_SUCCEEDED)
    {
      delete candidates_->at(i);
    }
  }

  emit ready(cand_rtp, cand_rtcp, sessionID_);
}
