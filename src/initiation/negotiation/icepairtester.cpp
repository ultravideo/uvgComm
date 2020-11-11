#include "icepairtester.h"

#include "stunmessage.h"
#include "common.h"

#include <QEventLoop>

const int STUN_RETRIES = 20;
const int STUN_RESPONSE_RETRIES = 3;
const int STUN_WAIT_MS = 20;

const int STUN_NOMINATION_RETRIES = 25;
const int STUN_NOMINATION_RESPONSE_WAITS = 128;
const int STUN_NOMINATION_RESPONSE_RETRIES = 5;


IcePairTester::IcePairTester(UDPServer* server):
  pair_(nullptr),
  controller_(false),
  udp_(server),
  stunmsg_(),
  interrupted_(false)
{}


IcePairTester::~IcePairTester()
{}


QHostAddress IcePairTester::getLocalAddress(std::shared_ptr<ICEInfo> info)
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


quint16 IcePairTester::getLocalPort(std::shared_ptr<ICEInfo> info)
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

void IcePairTester::setCandidatePair(std::shared_ptr<ICEPair> pair)
{
  Q_ASSERT(pair != nullptr);

  pair_ = pair;
}


void IcePairTester::setController(bool controller)
{
  controller_ = controller;
}


void IcePairTester::quit()
{
  interrupted_ = true;
  emit stopEventLoop();
  QThread::quit();
}


void IcePairTester::run()
{
  if (pair_ == nullptr)
  {
    printProgramError(this, "Unable to test connection, candidate is NULL!");
    return;
  }
  QString debug_type = "Controller";
  if (!controller_)
  {
    debug_type = "Controllee";
  }

  QString debugPath = pair_->local->address + ":" + QString::number(pair_->local->port) + " <-> " +
      pair_->remote->address + ":" + QString::number(pair_->remote->port);


  pair_->state = PAIR_IN_PROGRESS;

  printNormal(this, debug_type + " starts testing.", {"Pair"}, {debugPath});

  // TODO: I believe the testing should be identical for controller and controllee at binding phase

  // binding phase
  if (controller_)
  {
    if (!controllerBinding(pair_.get()))
    {
      printNormal(this, debug_type + " binding failed. No connection found.", {"Path"}, {debugPath});
      return; // failed
    }
  }
  else
  {
    if (!controlleeBinding(pair_.get()))
    {
      printNormal(this, debug_type + " binding failed. No connection found.", {"Path"}, {debugPath});
      return; // failed
    }
  }

  pair_->state = PAIR_SUCCEEDED;

  printNormal(this, debug_type + " binding succeeded", {"Pair"}, {debugPath});

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
      printNormal(this,  debug_type + " did not receive nomination for candidate: ", {"Pair"},
                  {debugPath});
      pair_->state = PAIR_FAILED;
      return; // failed
    }

    printNormal(this, debug_type + " nomination succeeded", {"Pair"}, {debugPath});

    emit controlleeNominationDone(pair_);
  }
}


bool IcePairTester::waitForStunRequest(unsigned long timeout)
{
  return waitForStunMessage(timeout, true, false, false);
}


bool IcePairTester::waitForStunResponse(unsigned long timeout)
{
  return waitForStunMessage(timeout, false, true, false);
}


bool IcePairTester::waitForStunNomination(unsigned long timeout)
{
  return waitForStunMessage(timeout, false, false, true);
}


bool IcePairTester::waitForStunMessage(unsigned long timeout,
                                       bool expectingRequest,
                                       bool expectingResponse,
                                       bool expectingNomination)
{
  QTimer timer;
  timer.setSingleShot(true);
  QEventLoop loop;

  if (expectingRequest)
  {
    connect(this,   &IcePairTester::requestRecv,   &loop, &QEventLoop::quit, Qt::DirectConnection);
  }

  if (expectingResponse)
  {
    connect(this,   &IcePairTester::responseRecv,   &loop, &QEventLoop::quit, Qt::DirectConnection);
  }

  if (expectingNomination)
  {
    connect(this,   &IcePairTester::nominationRecv, &loop, &QEventLoop::quit, Qt::DirectConnection);
  }

  connect(this,   &IcePairTester::stopEventLoop, &loop, &QEventLoop::quit, Qt::DirectConnection);
  connect(&timer, &QTimer::timeout,     &loop, &QEventLoop::quit, Qt::DirectConnection);

  timer.start(timeout);
  loop.exec();

  return timer.isActive();
}



bool IcePairTester::controllerBinding(ICEPair *pair)
{
  Q_ASSERT(pair != nullptr);

  // if we're the controlling agent, sending binding request is rather straight-forward:
  // send request, wait for response and return

  STUNMessage msg = stunmsg_.createRequest();
  msg.addAttribute(STUN_ATTR_ICE_CONTROLLING);
  msg.addAttribute(STUN_ATTR_PRIORITY, pair->priority);

  // we expect a response to this message from remote, by caching the TransactionID
  // and associated address/port pair, we can succesfully validate the response's TransactionID
  stunmsg_.expectReplyFrom(msg, pair->remote->address, pair->remote->port);

  QByteArray message = stunmsg_.hostToNetwork(msg);

  if (!sendRequestWaitResponse(pair, message, STUN_RETRIES, STUN_WAIT_MS))
  {
    printWarning(this, "Binding failed");
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

      msg = stunmsg_.createResponse();
      msg.addAttribute(STUN_ATTR_ICE_CONTROLLING);

      message     = stunmsg_.hostToNetwork(msg);
      msgReceived = true;

      for (int k = 0; k < STUN_RESPONSE_RETRIES; ++k)
      {
        if (!udp_->sendData(message,
                            getLocalAddress(pair->local),
                            QHostAddress(pair->remote->address),
                            pair->remote->port))
        {
          break;
        }

        // wait until sending the response again
        QThread::msleep(STUN_WAIT_MS);
      }
      break;
    }
  }

  return msgReceived;
}


bool IcePairTester::controlleeBinding(ICEPair *pair)
{
  Q_ASSERT(pair != nullptr);

  //TODO: Should the checks be performed at the same time?
  // Dummy packet sending so the port can be hole-punched.
  bool msgReceived   = false;
  STUNMessage msg    = stunmsg_.createRequest();
  QByteArray message = stunmsg_.hostToNetwork(msg);

  for (int i = 0; i < STUN_RETRIES; ++i)
  {
    if(!udp_->sendData(
      message,
      getLocalAddress(pair->local),
      QHostAddress(pair->remote->address),
      pair->remote->port))
    {
      break;
    }

    if (waitForStunRequest(STUN_WAIT_MS * (i + 1)))
    {
      if (interrupted_)
      {
        return false;
      }

      // send response to received request
      msg = stunmsg_.createResponse();
      msg.addAttribute(STUN_ATTR_ICE_CONTROLLED);

      message     = stunmsg_.hostToNetwork(msg);
      msgReceived = true;

      for (int k = 0; k < STUN_RESPONSE_RETRIES; ++k)
      {
        udp_->sendData(message,
                       getLocalAddress(pair->local),
                       QHostAddress(pair->remote->address),
                       pair->remote->port);

        QThread::msleep(STUN_WAIT_MS);
      }

      break;
    }
  }

  if (!msgReceived)
  {
    printWarning(this, "Failed to receive STUN Binding Request from remote!", {"Path"},
          {pair->local->address + ":" + QString::number(pair->local->port) + " <- " +
           pair->remote->address + ":" + QString::number(pair->remote->port)});

    return false;
  }

  // the first part of connective check is done (remote sending us binding request)
  // now we must do the same but this we're sending the request and they're responding

  // we've succesfully responded to remote's binding request, now it's our turn to
  // send request and remote must respond to them
  STUNMessage request = stunmsg_.createRequest();
  request.addAttribute(STUN_ATTR_ICE_CONTROLLED);
  request.addAttribute(STUN_ATTR_PRIORITY, pair->priority);

  message = stunmsg_.hostToNetwork(request);

  // we're expecting a response from remote to this request
  stunmsg_.cacheRequest(request);

  // do the connectivity check from our end to theirs
  msgReceived = sendRequestWaitResponse(pair, message, STUN_RETRIES, STUN_WAIT_MS);

  return msgReceived;
}


bool IcePairTester::sendNominationWaitResponse(ICEPair *pair)
{
  Q_ASSERT(pair != nullptr);

  STUNMessage request = stunmsg_.createRequest();
  request.addAttribute(STUN_ATTR_ICE_CONTROLLING);
  request.addAttribute(STUN_ATTR_USE_CANDIDATE);

  // expect reply for this message from remote
  stunmsg_.expectReplyFrom(request, pair->remote->address, pair->remote->port);

  QByteArray message  = stunmsg_.hostToNetwork(request);

  bool responseRecv = sendRequestWaitResponse(pair, message,
                                              STUN_NOMINATION_RETRIES,
                                              STUN_WAIT_MS);

  return responseRecv;
}


bool IcePairTester::waitNominationSendResponse(ICEPair *pair)
{
  Q_ASSERT(pair != nullptr);

  // dummy message. TODO: Is this needed?
  bool nominationRecv = false;
  STUNMessage msg     = stunmsg_.createRequest();
  QByteArray message = stunmsg_.hostToNetwork(msg);

  for (int i = 0; i < STUN_NOMINATION_RESPONSE_WAITS; ++i)
  {
    udp_->sendData(message,
                   getLocalAddress(pair->local),
                   QHostAddress(pair->remote->address),
                   pair->remote->port);

    if (waitForStunNomination(STUN_WAIT_MS * (i + 1)))
    {
      if (interrupted_)
      {
        return false;
      }

      msg = stunmsg_.createResponse();
      msg.addAttribute(STUN_ATTR_ICE_CONTROLLED);

      message = stunmsg_.hostToNetwork(msg);
      nominationRecv = true;

      for (int i = 0; i < STUN_NOMINATION_RESPONSE_RETRIES; ++i)
      {
        udp_->sendData(message,
                       getLocalAddress(pair->local),
                       QHostAddress(pair->remote->address),
                       pair->remote->port);

        QThread::msleep(STUN_WAIT_MS);
      }

      break;
    }
  }

  if (nominationRecv == false)
  {
    printWarning(this, "Failed to receive STUN Nomination Request from remote!", {"Remote"}, {
                   pair->remote->address + ":" + QString(pair->remote->port)});
    return false;
  }

  return nominationRecv;
}


// either we got Stun binding request -> send binding response
// or Stun binding response -> mark candidate as valid
void IcePairTester::recvStunMessage(QNetworkDatagram message)
{
  QByteArray data     = message.data();
  STUNMessage stunMsg;
  if (!stunmsg_.networkToHost(data, stunMsg))
  {
    return;
  }

  if (stunMsg.getType() == STUN_REQUEST)
  {
    if (stunmsg_.validateStunRequest(stunMsg))
    {
      if (controller_ && !stunMsg.hasAttribute(STUN_ATTR_ICE_CONTROLLED))
      {
        printWarning(this, "Got request without controlled attribute and we are the controller");
        return; // fail
      }
      else if (!controller_ && !stunMsg.hasAttribute(STUN_ATTR_ICE_CONTROLLING))
      {
        printWarning(this, "Got request without controlling attribute and we are not the controller");
        return; // fail
      }

      stunmsg_.cacheRequest(stunMsg);

      if (stunMsg.hasAttribute(STUN_ATTR_USE_CANDIDATE))
      {
        if (pair_->state != PAIR_SUCCEEDED)
        {
          printError(this, "Pair in wrong state when receiving nomination request");
          return; // fail
        }

        emit nominationRecv();
      }
      else
      {
        if (pair_->state != PAIR_IN_PROGRESS)
        {
          printError(this, "Pair in wrong state when receiving binding request");
          return; // fail
        }

        emit requestRecv();
      }
    }
    else
    {
      printWarning(this, "Received invalid STUN request in ice");
    }
  }
  else if (stunMsg.getType() == STUN_RESPONSE)
  {
    if (stunmsg_.validateStunResponse(stunMsg,
                                      message.senderAddress(),
                                      message.senderPort()))
    {
      emit responseRecv();
    }
    else
    {
      printWarning(this, "Received invalid STUN response in ICE");
    }
  }
  else
  {
     printDebug(DEBUG_WARNING, this,  "Received message with unknown type", {
                  "type", "from", "to" }, {
                  QString::number(stunMsg.getType()),
                  message.senderAddress().toString() + ":" +
                  QString::number(message.senderPort()),
                  message.destinationAddress().toString() + ":" +
                  QString::number(message.destinationPort())});
  }
}


bool IcePairTester::sendRequestWaitResponse(ICEPair* pair, QByteArray& request,
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
    printWarning(this, "Failed to receive STUN Binding Response from remote!", {"Remote"}, {
                   pair->remote->address + ":" + QString::number(pair->remote->port)});
  }

  return msgReceived;
}
