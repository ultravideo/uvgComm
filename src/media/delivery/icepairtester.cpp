#include "icepairtester.h"

#include "stunmessage.h"

#include "common.h"
#include "logger.h"

#include <QEventLoop>
#include <memory>

const int STUN_RETRIES = 20;
const int STUN_RESPONSE_RETRIES = 3;
const int STUN_WAIT_MS = 20;

const int STUN_NOMINATION_RETRIES = 25;
const int STUN_NOMINATION_RESPONSE_WAITS = 128;
const int STUN_NOMINATION_RESPONSE_RETRIES = 5;


ICEPairTester::ICEPairTester(UDPServer* server):
  pair_(nullptr),
  debugPair_(""),
  controller_(false),
  debugType_("Controllee"),
  udp_(server),
  stunOutTransaction_(),
  stunInTransaction_(),
  interrupted_(false)
{}


ICEPairTester::~ICEPairTester()
{}


QHostAddress ICEPairTester::getLocalAddress(std::shared_ptr<ICEInfo> info)
{
  // use relay address
  if (info->type != "host" &&
      info->rel_address != "" &&
      info->rel_port != 0)
  {
    return QHostAddress(info->rel_address);
  }

  // don't use relay address
  return QHostAddress(info->address);
}


quint16 ICEPairTester::getLocalPort(std::shared_ptr<ICEInfo> info)
{
  // use relay port
  if (info->type != "host" &&
      info->rel_address != "" &&
      info->rel_port != 0)
  {
    return info->rel_port;
  }

  // don't use relay port
  return info->port;
}

void ICEPairTester::setCandidatePair(std::shared_ptr<ICEPair> pair)
{
  Q_ASSERT(pair != nullptr);

  pair_ = pair;
  debugPair_ = pair_->local->address + ":" + QString::number(pair_->local->port) + " <-> " +
      pair_->remote->address + ":" + QString::number(pair_->remote->port);
}


void ICEPairTester::setController(bool controller)
{
  controller_ = controller;
  if (controller)
  {
    debugType_ = "Controller";
  }
  else
  {
    debugType_ = "Controllee";
  }
}


void ICEPairTester::quit()
{
  interrupted_ = true;
  emit stopEventLoop();
  QThread::quit();
}


void ICEPairTester::run()
{
  if (pair_ == nullptr)
  {
    Logger::getLogger()->printProgramError(this, "Unable to test connection, "
                                                 "candidate is NULL!");
    return;
  }

  pair_->state = PAIR_IN_PROGRESS;

  Logger::getLogger()->printNormal(this, debugType_ + " starts connectivity tests.", 
                                   {"Pair"}, {debugPair_});

  // binding phase
  if (controller_)
  {
    if (!controllerBinding(pair_.get()))
    {
      return; // failed or interrupted
    }
  }
  else
  {
    if (!controlleeBinding(pair_.get()))
    {
      return; // failed or interrupted
    }
  }

  pair_->state = PAIR_SUCCEEDED;

  Logger::getLogger()->printNormal(this, debugType_ + " found a connection", 
                                   {"Pair"}, {debugPair_});

  // controller performs the nomination process separately so we exit
  if (controller_)
  {
    // we have to sync the nomination elsewhere because only one pair should be nominated
    emit controllerPairSucceeded(pair_);
  }
  else
  {
    // controllee starts waiting for nomination requests on this pair.
    if (!waitNominationSendResponse(pair_.get()))
    {
      pair_->state = PAIR_FAILED;
      return; // failed or interrupted
    }

    Logger::getLogger()->printNormal(this, debugType_ + " nomination succeeded", 
                                     {"Pair"}, {debugPair_});

    emit controlleeNominationDone(pair_);
  }
}


bool ICEPairTester::waitForStunRequest(unsigned long timeout)
{
  return waitForStunMessage(timeout, true, false, false);
}


bool ICEPairTester::waitForStunResponse(unsigned long timeout)
{
  return waitForStunMessage(timeout, false, true, false);
}


bool ICEPairTester::waitForStunNomination(unsigned long timeout)
{
  return waitForStunMessage(timeout, false, false, true);
}


bool ICEPairTester::waitForStunMessage(unsigned long timeout,
                                       bool expectingRequest,
                                       bool expectingResponse,
                                       bool expectingNomination)
{
  QTimer timer;
  timer.setSingleShot(true);
  QEventLoop loop;

  if (expectingRequest)
  {
    connect(this,   &ICEPairTester::requestRecv,   &loop, &QEventLoop::quit, Qt::DirectConnection);
  }

  if (expectingResponse)
  {
    connect(this,   &ICEPairTester::responseRecv,   &loop, &QEventLoop::quit, Qt::DirectConnection);
  }

  if (expectingNomination)
  {
    connect(this,   &ICEPairTester::nominationRecv, &loop, &QEventLoop::quit, Qt::DirectConnection);
  }

  connect(this,   &ICEPairTester::stopEventLoop, &loop, &QEventLoop::quit, Qt::DirectConnection);
  connect(&timer, &QTimer::timeout,     &loop, &QEventLoop::quit, Qt::DirectConnection);

  timer.start(timeout);
  loop.exec();

  return timer.isActive();
}



bool ICEPairTester::controllerBinding(ICEPair *pair)
{
  Q_ASSERT(pair != nullptr);

  // if we're the controlling agent, sending binding request is rather straight-forward:
  // send request, wait for response and return

  STUNMessage msg = stunOutTransaction_.createRequest();
  msg.addAttribute(STUN_ATTR_ICE_CONTROLLING);
  msg.addAttribute(STUN_ATTR_PRIORITY, pair->priority);

  // we expect a response to this message from remote, by caching the TransactionID
  // and associated address/port pair, we can succesfully validate the response's TransactionID
  stunOutTransaction_.expectReplyFrom(msg, pair->remote->address, pair->remote->port);

  QByteArray message = stunOutTransaction_.hostToNetwork(msg);

  if (!sendRequestWaitResponse(pair, message, STUN_RETRIES, STUN_WAIT_MS))
  {
    return false;
  }

  // the first part of connectivity check is done, now we must wait for
  // remote's binding request and responed to them
  bool msgReceived = false;

  for (int i = 0; i < STUN_RETRIES; ++i)
  {
    if (waitForStunRequest(STUN_WAIT_MS * (i + 1)))
    {
      if (interrupted_)
      {
        return false;
      }

      msg = stunInTransaction_.createResponse();
      msg.addAttribute(STUN_ATTR_ICE_CONTROLLING);

      message     = stunInTransaction_.hostToNetwork(msg);
      msgReceived = true;

      for (int k = 0; k < STUN_RESPONSE_RETRIES; ++k)
      {
        if (!udp_->sendData(message, getLocalAddress(pair->local),
                            QHostAddress(pair->remote->address), pair->remote->port))
        {
          Logger::getLogger()->printNormal(this, debugType_ + 
                                                 " failed to send STUN Binding request!",
                                           {"Pair"}, {debugPair_});

          return false;
        }

        // wait until sending the response again
        QThread::msleep(STUN_WAIT_MS);
      }
      break;
    }
  }

  if (!msgReceived)
  {
    Logger::getLogger()->printNormal(this, debugType_ + " did not receive "
                                           "STUN binding request from remote!",
                                     {"Pair"}, {debugPair_});
  }

  return msgReceived;
}


bool ICEPairTester::controlleeBinding(ICEPair *pair)
{
  Q_ASSERT(pair != nullptr);

  //TODO: Should the checks be performed at the same time? To me the need for dummy packets makes little
  // sense to me, the RFC must have a way to handle this situation differently

  // Dummy packet sending so the port can be hole-punched.
  bool msgReceived   = false;
  STUNMessage msg    = stunOutTransaction_.createRequest();
  QByteArray message = stunOutTransaction_.hostToNetwork(msg);

  for (int i = 0; i < STUN_RETRIES; ++i)
  {
    if(!udp_->sendData(message,  getLocalAddress(pair->local),
                       QHostAddress(pair->remote->address), pair->remote->port))
    {
      Logger::getLogger()->printWarning(this, debugType_ + " failed to send dummy packet", 
                                        {"Pair"}, {debugPair_});
      return false;
    }

    if (waitForStunRequest(STUN_WAIT_MS * (i + 1)))
    {
      if (interrupted_)
      {
        return false;
      }

      // send response to received request
      msg = stunInTransaction_.createResponse();
      msg.addAttribute(STUN_ATTR_ICE_CONTROLLED);

      message     = stunInTransaction_.hostToNetwork(msg);
      msgReceived = true;

      for (int k = 0; k < STUN_RESPONSE_RETRIES; ++k)
      {
        if (!udp_->sendData(message,  getLocalAddress(pair->local),
                            QHostAddress(pair->remote->address), pair->remote->port))
        {
          Logger::getLogger()->printNormal(this, debugType_ + 
                                                " failed to send STUN Binding response!",
                                           {"Pair"}, {debugPair_});
          return false;
        }

        QThread::msleep(STUN_WAIT_MS);
      }

      break;
    }
  }

  if (!msgReceived)
  {
    Logger::getLogger()->printNormal(this, "Failed to receive STUN Binding Request from remote!", 
                                    {"Pair"}, {debugPair_});

    return false;
  }

  // the first part of connective check is done (remote sending us binding request)
  // now we must do the same but this we're sending the request and they're responding

  // we've succesfully responded to remote's binding request, now it's our turn to
  // send request and remote must respond to them
  STUNMessage request = stunOutTransaction_.createRequest();
  request.addAttribute(STUN_ATTR_ICE_CONTROLLED);
  request.addAttribute(STUN_ATTR_PRIORITY, pair->priority);

  message = stunOutTransaction_.hostToNetwork(request);

  // we're expecting a response from remote to this request
  stunOutTransaction_.cacheRequest(request);

  // do the connectivity check from our end to theirs
  msgReceived = sendRequestWaitResponse(pair, message, STUN_RETRIES, STUN_WAIT_MS);

  return msgReceived;
}


bool ICEPairTester::sendNominationWaitResponse(ICEPair *pair)
{
  Q_ASSERT(pair != nullptr);

  STUNMessage request = stunOutTransaction_.createRequest();
  request.addAttribute(STUN_ATTR_ICE_CONTROLLING);
  request.addAttribute(STUN_ATTR_USE_CANDIDATE);

  // expect reply for this message from remote
  stunOutTransaction_.expectReplyFrom(request, pair->remote->address, pair->remote->port);

  QByteArray message  = stunOutTransaction_.hostToNetwork(request);

  bool responseRecv = sendRequestWaitResponse(pair, message,
                                              STUN_NOMINATION_RETRIES,
                                              STUN_WAIT_MS);

  return responseRecv;
}


bool ICEPairTester::waitNominationSendResponse(ICEPair *pair)
{
  Q_ASSERT(pair != nullptr);

  // dummy message. TODO: Is this needed?
  bool nominationRecv = false;
  STUNMessage msg     = stunOutTransaction_.createRequest();
  QByteArray message = stunOutTransaction_.hostToNetwork(msg);

  for (int i = 0; i < STUN_NOMINATION_RESPONSE_WAITS; ++i)
  {
    if (!udp_->sendData(message,  getLocalAddress(pair->local),
                        QHostAddress(pair->remote->address), pair->remote->port))
    {
      Logger::getLogger()->printWarning(this, debugType_ + " failed to send "
                                              "dummy packet in nomination",
                                        {"Pair"}, {debugPair_});
      return false;
    }

    if (waitForStunNomination(STUN_WAIT_MS * (i + 1)))
    {
      if (interrupted_)
      {
        return false;
      }

      msg = stunInTransaction_.createResponse();
      msg.addAttribute(STUN_ATTR_ICE_CONTROLLED);

      message = stunInTransaction_.hostToNetwork(msg);
      nominationRecv = true;

      for (int i = 0; i < STUN_NOMINATION_RESPONSE_RETRIES; ++i)
      {
        if (!udp_->sendData(message,  getLocalAddress(pair->local),
                            QHostAddress(pair->remote->address), pair->remote->port))
        {
          Logger::getLogger()->printWarning(this, debugType_ + 
                                                 " failed to send nomination response",
                                            {"Pair"}, {debugPair_});
          return false;
        }

        QThread::msleep(STUN_WAIT_MS);
      }

      break;
    }
  }

  if (nominationRecv == false)
  {
    Logger::getLogger()->printNormal(this, "Failed to receive STUN Nomination Request "
                                           "from remote!", {"Pair"}, {debugPair_});
    return false;
  }

  return nominationRecv;
}


// either we got Stun binding request -> send binding response
// or Stun binding response -> mark candidate as valid
void ICEPairTester::recvStunMessage(QNetworkDatagram message)
{
  QByteArray data     = message.data();
  STUNMessage stunMsg;
  if (!stunInTransaction_.networkToHost(data, stunMsg))
  {
    return;
  }

  if (stunMsg.getType() == STUN_REQUEST)
  {
    if (stunInTransaction_.validateStunRequest(stunMsg))
    {
      if (controller_ && !stunMsg.hasAttribute(STUN_ATTR_ICE_CONTROLLED))
      {
        // TODO: When dummy packets are removed, enable this print
        //Logger::getLogger()->printWarning(this, "Got request without controlled attribute "
        //                                        "and we are the controller");
        return; // fail
      }
      else if (!controller_ && !stunMsg.hasAttribute(STUN_ATTR_ICE_CONTROLLING))
      {
        // TODO: When dummy packets are removed, enable this print
        //Logger::getLogger()->printWarning(this, "Got request without controlling attribute "
        //                                        "and we are not the controller");
        return; // fail
      }

      stunInTransaction_.cacheRequest(stunMsg);

      if (stunMsg.hasAttribute(STUN_ATTR_USE_CANDIDATE))
      {
        if (pair_->state != PAIR_SUCCEEDED)
        {
          Logger::getLogger()->printWarning(this, "Pair is not in succeeded state when receiving nomination request",
                                                  "State", stateToString(pair_->state));
          return; // fail
        }

        emit nominationRecv();
      }
      else
      {
        if (pair_->state != PAIR_IN_PROGRESS)
        {
          Logger::getLogger()->printWarning(this, "Pair is not in progress state "
                                                  "when receiving binding request",
                                            "State", stateToString(pair_->state));
          return; // fail
        }

        emit requestRecv();
      }
    }
    else
    {
      Logger::getLogger()->printWarning(this, "Received invalid STUN request in ice");
    }
  }
  else if (stunMsg.getType() == STUN_RESPONSE)
  {
    if (stunOutTransaction_.validateStunResponse(stunMsg,
                                                 message.senderAddress(),
                                                 message.senderPort()))
    {
      emit responseRecv();
    }
  }
  else
  {
     Logger::getLogger()->printDebug(DEBUG_WARNING, this,  
                                     "Received message with unknown type", 
                                     {"type", "from", "to" }, {
                                     QString::number(stunMsg.getType()),
                                     message.senderAddress().toString() + ":" +
                                     QString::number(message.senderPort()),
                                     message.destinationAddress().toString() + ":" +
                                     QString::number(message.destinationPort())});
  }
}


bool ICEPairTester::sendRequestWaitResponse(ICEPair* pair, QByteArray& request,
                                            int retries, int baseTimeout)
{
  bool msgReceived = false;
  for (int i = 0; i < retries; ++i)
  {
    if(!udp_->sendData(request,
                       getLocalAddress(pair->local),
                       QHostAddress(pair->remote->address),
                       pair->remote->port))
    {
      Logger::getLogger()->printWarning(this, debugType_ + " failed to send dummy packet", 
                                       {"Pair"}, {debugPair_});
      return false;
    }

    if (waitForStunResponse(baseTimeout * (i + 1)))
    {
      if (interrupted_)
      {
        return false;
      }

      msgReceived = true;
      break;
    }
  }

  if (!msgReceived)
  {
    Logger::getLogger()->printNormal(this, debugType_ + " failed to receive "
                                           "STUN Binding Response from remote!",
                                     {"Pair"}, {debugPair_});
  }

  return msgReceived;
}


QString ICEPairTester::stateToString(PairState state)
{
  QString string = "";
  switch (state)
  {
    case PAIR_WAITING:
    {
      string = "Waiting";
      break;
    }
    case PAIR_IN_PROGRESS:
    {
      string = "In Progress";
      break;
    }
    case PAIR_SUCCEEDED:
    {
      string = "Succeeded";
      break;
    }
    case PAIR_FAILED:
    {
      string = "Failed";
      break;
    }
    case PAIR_FROZEN:
    {
      string = "Frozen";
      break;
    }
    case PAIR_NOMINATED:
    {
      string = "Nominated";
      break;
    }
    default:
    {
      string = "Unknown";
      break;
    }
  }

  return string;
}
