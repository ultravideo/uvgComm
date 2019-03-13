#include "iceflowcontrol.h"
#include "ice.h"

FlowAgent::FlowAgent():
  candidates_(nullptr)
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

void FlowController::run()
{
  if (candidates_ == nullptr || candidates_->size() == 0)
  {
    qDebug() << "ERROR: invalid candidates, unable to perform ICE candidate negotiation!";
    emit ready(nullptr, nullptr);
  }

  Stun stun; 
  QList<ICEPair *> validPairs;

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
        break;
      }
    }

    candidates_->at(i + 0)->state = PAIR_FAILED;
    candidates_->at(i + 1)->state = PAIR_FAILED;
  }

  if (validPairs.size() < 2)
  {
    qDebug() << "ERROR: Failed to negotiate a list of valid candidates with remote!";
    emit ready(nullptr, nullptr);
  }

  qDebug() << "[controller] STARTING NOMINATION!";

  // nominate RTP candidate
  if (!stun.sendNominationRequest(validPairs[0]->remote->address, validPairs[0]->remote->port,
                                  validPairs[0]->local->address,  validPairs[0]->local->port))
  {
    qDebug() << "[controller] RTP CANDIDATE FAILED";
    emit ready(nullptr, nullptr);
    return;
  }

  // nominate RTCP candidate
  if (!stun.sendNominationRequest(validPairs[1]->remote->address, validPairs[1]->remote->port,
                                  validPairs[1]->local->address,  validPairs[1]->local->port))
  {
    qDebug() << "[controller] RTCP CANDIDATE FAILED";
    emit ready(nullptr, nullptr);
    return;
  }

  qDebug() << "[controller] NOMINATION OK!";
  emit ready(validPairs[0], validPairs[1]);
}

void FlowControllee::run()
{
  if (candidates_ == nullptr || candidates_->size() == 0)
  {
    qDebug() << "ERROR: invalid candidates, unable to perform ICE candidate negotiation!";
    emit ready(nullptr, nullptr);
  }

  Stun stun; 
  QList<ICEPair *> validPairs;

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
        break;
      }
    }

    candidates_->at(i + 0)->state = PAIR_FAILED;
    candidates_->at(i + 1)->state = PAIR_FAILED;
  }

  if (validPairs.size() < 2)
  {
    qDebug() << "ERROR: Failed to negotiate a list of valid candidates with remote!";
    emit ready(nullptr, nullptr);
  }

  qDebug() << "[controllee] RESPONDING TO NOMINATION REQUEST!";

  // respond to RTP nomination
  if (!stun.sendNominationResponse(validPairs[0]->remote->address, validPairs[0]->remote->port,
                                  validPairs[0]->local->address,  validPairs[0]->local->port))
  {
    qDebug() << "[controllee] RTP CANDIDATE FAILED";
    emit ready(nullptr, nullptr);
    return;
  }

  // respond to RTCP nomination
  if (!stun.sendNominationResponse(validPairs[1]->remote->address, validPairs[1]->remote->port,
                                  validPairs[1]->local->address,  validPairs[1]->local->port))
  {
    qDebug() << "[controllee] RTCP CANDIDATE FAILED";
    emit ready(nullptr, nullptr);
    return;
  }

  qDebug() << "[controllee] NOMINATION OK!";
  emit ready(validPairs[0], validPairs[1]);
}
