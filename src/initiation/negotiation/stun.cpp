#include "stun.h"

#include "stunmessagefactory.h"
#include "common.h"

#include <QDnsLookup>
#include <QHostInfo>
#include <QDateTime>
#include <QMutex>
#include <QWaitCondition>
#include <QEventLoop>
#include <QThread>

#include <QtEndian>

#include <QNetworkInterface>


Stun::Stun():
  udp_(new UDPServer),
  stunmsg_(),
  multiplex_(false),
  interrupted_(false)
{}


Stun::Stun(UDPServer *server):
  udp_(server),
  stunmsg_(),
  multiplex_(true),
  interrupted_(false)
{}


bool Stun::waitForStunResponse(unsigned long timeout)
{
  QTimer timer;
  timer.setSingleShot(true);
  QEventLoop loop;

  connect(this,   &Stun::parsingDone,   &loop, &QEventLoop::quit, Qt::DirectConnection);
  connect(this,   &Stun::stopEventLoop, &loop, &QEventLoop::quit, Qt::DirectConnection);
  connect(&timer, &QTimer::timeout,     &loop, &QEventLoop::quit, Qt::DirectConnection);

  timer.start(timeout);
  loop.exec();

  return timer.isActive();
}


bool Stun::waitForStunRequest(unsigned long timeout)
{
  QTimer timer;
  timer.setSingleShot(true);
  QEventLoop loop;

  connect(this,   &Stun::requestRecv,   &loop, &QEventLoop::quit, Qt::DirectConnection);
  connect(this,   &Stun::stopEventLoop, &loop, &QEventLoop::quit, Qt::DirectConnection);
  connect(&timer, &QTimer::timeout,     &loop, &QEventLoop::quit, Qt::DirectConnection);

  timer.start(timeout);
  loop.exec();

  return timer.isActive();
}


bool Stun::waitForNominationRequest(unsigned long timeout)
{
  QTimer timer;
  timer.setSingleShot(true);
  QEventLoop loop;

  connect(this,   &Stun::nominationRecv, &loop, &QEventLoop::quit, Qt::DirectConnection);
  connect(this,   &Stun::stopEventLoop,  &loop, &QEventLoop::quit, Qt::DirectConnection);
  connect(&timer, &QTimer::timeout,      &loop, &QEventLoop::quit, Qt::DirectConnection);

  timer.start(timeout);
  loop.exec();

  return timer.isActive();
}


// if we're the controlling agent, sending binding request is rather straight-forward:
// send request, wait for response and return
bool Stun::controllerSendBindingRequest(ICEPair *pair)
{
  Q_ASSERT(pair != nullptr);

  STUNMessage msg = stunmsg_.createRequest();
  msg.addAttribute(STUN_ATTR_ICE_CONTROLLING);
  msg.addAttribute(STUN_ATTR_PRIORITY, pair->priority);

  // we expect a response to this message from remote, by caching the TransactionID
  // and associated address/port pair, we can succesfully validate the response's TransactionID
  stunmsg_.expectReplyFrom(msg, pair->remote->address, pair->remote->port);

  QByteArray message = stunmsg_.hostToNetwork(msg);

  if (!sendRequestWaitResponse(pair, message, 20, 20))
  {
    if (!multiplex_)
    {
      udp_->unbind();
    }
    return false;
  }

  // the first part of connectivity check is done, now we must wait for
  // remote's binding request and responed to them
  bool msgReceived = false;

  for (int i = 0; i < 20; ++i)
  {
    if (waitForStunRequest(20 * (i + 1)))
    {
      if (interrupted_)
      {
        if (!multiplex_)
        {
          udp_->unbind();
        }
        return false;
      }

      msg = stunmsg_.createResponse();
      msg.addAttribute(STUN_ATTR_ICE_CONTROLLING);

      message     = stunmsg_.hostToNetwork(msg);
      msgReceived = true;

      for (int k = 0; k < 3; ++k)
      {
        if (!udp_->sendData(message,
                            QHostAddress(pair->local->address),
                            QHostAddress(pair->remote->address),
                            pair->remote->port))
        {
          break;
        }
        QThread::msleep(20);
      }
      break;
    }
  }

  if (multiplex_ == false)
  {
    udp_->unbind();
  }
  return msgReceived;
}


bool Stun::controlleeSendBindingRequest(ICEPair *pair)
{
  Q_ASSERT(pair != nullptr);

  bool msgReceived   = false;
  STUNMessage msg    = stunmsg_.createRequest();
  QByteArray message = stunmsg_.hostToNetwork(msg);

  for (int i = 0; i < 20; ++i)
  {
    if(!udp_->sendData(
      message,
      QHostAddress(pair->local->address),
      QHostAddress(pair->remote->address),
      pair->remote->port))
    {
      break;
    }

    if (waitForStunRequest(20 * (i + 1)))
    {
      if (interrupted_)
      {
        return false;
      }

      msg = stunmsg_.createResponse();
      msg.addAttribute(STUN_ATTR_ICE_CONTROLLED);

      message     = stunmsg_.hostToNetwork(msg);
      msgReceived = true;

      for (int k = 0; k < 3; ++k)
      {
        udp_->sendData(message,
                       QHostAddress(pair->local->address),
                       QHostAddress(pair->remote->address),
                       pair->remote->port);

        QThread::msleep(20);
      }

      break;
    }
  }

  if (msgReceived)
  {
    printDebug(DEBUG_WARNING, "STUN",
        "Failed to receive STUN Binding Request from remote!", {"Path"},
    {pair->local->address + ":" + QString::number(pair->local->port) + " <- " +
     pair->remote->address + ":" + QString::number(pair->remote->port)});

    if (!multiplex_)
    {
      udp_->unbind();
    }
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

  msgReceived = sendRequestWaitResponse(pair, message, 20, 20);

  if (!multiplex_)
  {
    udp_->unbind();
  }
  return msgReceived;
}


bool Stun::sendBindingRequest(ICEPair *pair, bool controller)
{
  Q_ASSERT(pair != nullptr);

  if (controller)
  {
    printNormal(this, "Controller starts testing path.", {"Path"}, {
                  pair->local->address + ":" + QString::number(pair->local->port) + " -> " +
                  pair->remote->address + ":" + QString::number(pair->remote->port)});
  }
  else
  {
    printNormal(this, "Non-controller starts testing path.", {"Path"}, {
                  pair->local->address + ":" + QString::number(pair->local->port) + " -> " +
                  pair->remote->address + ":" + QString::number(pair->remote->port)});
  }

  if (!multiplex_)
  {
    if (!udp_->bindSocket(QHostAddress(pair->local->address), pair->local->port, false))
    {
      printDebug(DEBUG_ERROR, "STUN",
          "Binding failed! Cannot send STUN Binding Requests to", {
            pair->remote->address, QString(pair->remote->port)
          }
      );

      return false;
    }

    connect(udp_, &UDPServer::datagramAvailable, this, &Stun::recvStunMessage);
  }

  if (controller)
  {
    return controllerSendBindingRequest(pair);
  }

  return controlleeSendBindingRequest(pair);
}


bool Stun::sendNominationRequest(ICEPair *pair)
{
  Q_ASSERT(pair != nullptr);

  if (!multiplex_)
  {
    if (!udp_->bindSocket(QHostAddress(pair->local->address), pair->local->port, false))
    {
      printDebug(DEBUG_ERROR, "STUN",
          "Binding failed! Cannot send STUN Binding Requests to", {
            pair->remote->address, QString(pair->remote->port)
          }
      );

      return false;
    }

    connect(udp_, &UDPServer::datagramAvailable, this, &Stun::recvStunMessage);
  }

  STUNMessage request = stunmsg_.createRequest();
  request.addAttribute(STUN_ATTR_ICE_CONTROLLING);
  request.addAttribute(STUN_ATTR_USE_CANDIATE);

  // expect reply for this message from remote
  stunmsg_.expectReplyFrom(request, pair->remote->address, pair->remote->port);

  QByteArray message  = stunmsg_.hostToNetwork(request);

  bool responseRecv = sendRequestWaitResponse(pair, message, 25, 20);

  if (!multiplex_)
  {
    udp_->unbind();
  }
  return responseRecv;
}


bool Stun::sendNominationResponse(ICEPair *pair)
{
  Q_ASSERT(pair != nullptr);

  if (!multiplex_)
  {
    if (!udp_->bindSocket(QHostAddress(pair->local->address), pair->local->port, false))
    {
      printError(this, "Binding failed! Cannot send STUN Binding Requests to", {"Interface:"}, {
                   pair->remote->address + ":" + QString::number(pair->remote->port)});
      return false;
    }
    connect(udp_, &UDPServer::datagramAvailable, this, &Stun::recvStunMessage);
  }

  bool nominationRecv = false;
  STUNMessage msg     = stunmsg_.createRequest();
  QByteArray message = stunmsg_.hostToNetwork(msg);

  for (int i = 0; i < 128; ++i)
  {
    udp_->sendData(message,
                   QHostAddress(pair->local->address),
                   QHostAddress(pair->remote->address),
                   pair->remote->port);

    if (waitForNominationRequest(20 * (i + 1)))
    {
      if (interrupted_)
      {
        return false;
      }

      msg = stunmsg_.createResponse();
      msg.addAttribute(STUN_ATTR_ICE_CONTROLLED);

      message = stunmsg_.hostToNetwork(msg);
      nominationRecv = true;

      for (int i = 0; i < 5; ++i)
      {
        udp_->sendData(message,
                       QHostAddress(pair->local->address),
                       QHostAddress(pair->remote->address),
                       pair->remote->port);

        QThread::msleep(20);
      }

      break;
    }
  }

  if (nominationRecv == false)
  {
    printWarning(this, "Failed to receive STUN Nomination Request from remote!", {"Remote"}, {
                   pair->remote->address + ":" + QString(pair->remote->port)});

    if (!multiplex_)
    {
      udp_->unbind();
    }
    return false;
  }

  if (!multiplex_)
  {
    udp_->unbind();
  }

  return nominationRecv;
}


void Stun::stopTesting()
{
  interrupted_ = true;
  emit stopEventLoop();
}


// either we got Stun binding request -> send binding response
// or Stun binding response -> mark candidate as valid
void Stun::recvStunMessage(QNetworkDatagram message)
{
  QByteArray data     = message.data();
  STUNMessage stunMsg = stunmsg_.networkToHost(data);

  if (stunMsg.getType() == STUN_REQUEST)
  {
    if (stunmsg_.validateStunRequest(stunMsg))
    {
      stunmsg_.cacheRequest(stunMsg);

      if (stunMsg.hasAttribute(STUN_ATTR_USE_CANDIATE))
      {
        emit nominationRecv();
        return;
      }

      emit requestRecv();
    }
  }
  else if (stunMsg.getType() == STUN_RESPONSE)
  {
    if (stunmsg_.validateStunResponse(stunMsg,
                                      message.senderAddress(),
                                      message.senderPort()))
    {
      emit parsingDone();
    }
  }
  else
  {
     printDebug(DEBUG_WARNING, "STUN",  "Received message with unknown type", { "type", "from", "to" },
         { QString::number(stunMsg.getType()), message.senderAddress().toString() + ":" + QString::number(message.senderPort()),
           message.destinationAddress().toString() + ":" + QString::number(message.destinationPort())});
  }
}


bool Stun::sendRequestWaitResponse(ICEPair* pair, QByteArray& request,
                                   int retries, int baseTimeout)
{
  bool msgReceived = false;
  for (int i = 0; i < retries; ++i)
  {
    if(!udp_->sendData(request,
                       QHostAddress(pair->local->address),
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
                   pair->remote->address + ":" + QString(pair->remote->port)});
  }

  return msgReceived;
}
