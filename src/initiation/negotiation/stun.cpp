#include "common.h"
#include "stun.h"
#include "stunmsgfact.h"

#include <QDnsLookup>
#include <QHostInfo>
#include <QDateTime>
#include <QDebug>
#include <QMutex>
#include <QWaitCondition>
#include <QEventLoop>
#include <QThread>

#include <QtEndian>

const uint16_t GOOGLE_STUN_PORT = 19302;
const uint16_t STUN_PORT = 21000;

Stun::Stun():
  udp_(new UDPServer),
  stunmsg_(),
  multiplex_(false),
  interrupted_(false)
{
}

Stun::Stun(UDPServer *server):
  udp_(server),
  stunmsg_(),
  multiplex_(true),
  interrupted_(false)
{
}

void Stun::wantAddress(QString stunServer)
{
  // To find the IP address of qt-project.org
  QHostInfo::lookupHost(stunServer, this, SLOT(handleHostaddress(QHostInfo)));
}

void Stun::handleHostaddress(QHostInfo info)
{
  const auto addresses = info.addresses();
  QHostAddress address;
  if(addresses.size() != 0)
  {
    address = addresses.at(0);
  }
  else
  {
    return;
  }

  udp_->bind(QHostAddress::AnyIPv4, STUN_PORT);
  QObject::connect(
      udp_,
      SIGNAL(messageAvailable(QByteArray)),
      this,
      SLOT(processReply(QByteArray))
  );

  STUNMessage request = stunmsg_.createRequest();
  QByteArray message  = stunmsg_.hostToNetwork(request);

  stunmsg_.cacheRequest(request);

  udp_->sendData(message, address, GOOGLE_STUN_PORT, false);
}

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
  bool msgReceived   = false;

  for (int i = 0; i < 20; ++i)
  {
    udp_->sendData(
        message,
        QHostAddress(pair->remote->address),
        pair->remote->port,
        false
    );

    if (waitForStunResponse(20 * (i + 1)))
    {
      if (interrupted_)
      {
        return false;
      }

      msgReceived = true;
      break;
    }
  }

  if (msgReceived == false)
  {
    printDebug(DEBUG_WARNING, "STUN", DC_NEGOTIATING,
        "Failed to receive STUN Binding Response from remote!", {
          pair->remote->address, QString(pair->remote->port)
        }
    );

    if (multiplex_ == false)
    {
      udp_->unbind();
    }
    return false;
  }

  // the first part of connectivity check is done, now we must wait for
  // remote's binding request and responed to them
  msgReceived = false;

  for (int i = 0; i < 20; ++i)
  {
    if (waitForStunRequest(20 * (i + 1)))
    {
      if (interrupted_)
      {
        return false;
      }

      msg = stunmsg_.createResponse();
      msg.addAttribute(STUN_ATTR_ICE_CONTROLLING);

      message     = stunmsg_.hostToNetwork(msg);
      msgReceived = true;

      for (int k = 0; k < 3; ++k)
      {
        udp_->sendData(
            message,
            QHostAddress(pair->remote->address),
            pair->remote->port,
            false
        );
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
  QByteArray message = QByteArray("hole punch normal");
  STUNMessage msg;

  for (int i = 0; i < 20; ++i)
  {
    udp_->sendData(
        message,
        QHostAddress(pair->remote->address),
        pair->remote->port,
        false
    );

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
        udp_->sendData(
            message,
            QHostAddress(pair->remote->address),
            pair->remote->port,
            false
        );
        QThread::msleep(20);
      }

      break;
    }
  }

  if (msgReceived == false)
  {
    printDebug(DEBUG_WARNING, "STUN", DC_NEGOTIATING,
        "Failed to receive STUN Binding Request from remote!", {
          pair->remote->address, QString(pair->remote->port)
        }
    );

    if (multiplex_ == false)
    {
      udp_->unbind();
    }
    return false;
  }

  // the first part of connective check is done (remote sending us binding request)
  // now we must do the same but this we're sending the request and they're responding
  msgReceived = false;

  // we've succesfully responded to remote's binding request, now it's our turn to
  // send request and remote must respond to them
  STUNMessage request = stunmsg_.createRequest();
  request.addAttribute(STUN_ATTR_ICE_CONTROLLED);
  request.addAttribute(STUN_ATTR_PRIORITY, pair->priority);

  message = stunmsg_.hostToNetwork(request);

  // we're expecting a response from remote to this request
  stunmsg_.cacheRequest(request);

  for (int i = 0; i < 20; ++i)
  {
    udp_->sendData(
        message,
        QHostAddress(pair->remote->address),
        pair->remote->port,
        false
    );

    if (waitForStunResponse(20 * (i + 1)))
    {
      if (interrupted_)
      {
        return false;
      }

      msgReceived = true;
      break;
    }
  }

  if (multiplex_ == false)
  {
    udp_->unbind();
  }
  return msgReceived;
}

bool Stun::sendBindingRequest(ICEPair *pair, bool controller)
{
  Q_ASSERT(pair != nullptr);

  if (multiplex_ == false)
  {
    if (!udp_->bindRaw(QHostAddress(pair->local->address), pair->local->port))
    {
      printDebug(DEBUG_ERROR, "STUN", DC_NEGOTIATING,
          "Binding failed! Cannot send STUN Binding Requests to", {
            pair->remote->address, QString(pair->remote->port)
          }
      );

      return false;
    }

    connect(udp_, &UDPServer::rawMessageAvailable, this, &Stun::recvStunMessage);
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

  if (multiplex_ == false)
  {
    if (!udp_->bindRaw(QHostAddress(pair->local->address), pair->local->port))
    {
      printDebug(DEBUG_ERROR, "STUN", DC_NEGOTIATING,
          "Binding failed! Cannot send STUN Binding Requests to", {
            pair->remote->address, QString(pair->remote->port)
          }
      );

      return false;
    }

    connect(udp_, &UDPServer::rawMessageAvailable, this, &Stun::recvStunMessage);
  }

  STUNMessage request = stunmsg_.createRequest();
  request.addAttribute(STUN_ATTR_ICE_CONTROLLING);
  request.addAttribute(STUN_ATTR_USE_CANDIATE);

  // expect reply for this message from remote
  stunmsg_.expectReplyFrom(request, pair->remote->address, pair->remote->port);

  QByteArray message  = stunmsg_.hostToNetwork(request);

  bool responseRecv = false;

  for (int i = 0; i < 25; ++i)
  {
    udp_->sendData(
        message,
        QHostAddress(pair->remote->address),
        pair->remote->port,
        false
    );

    if (waitForStunResponse(20 * (i + 1)))
    {
      if (interrupted_)
      {
        return false;
      }

      responseRecv = true;
      break;
    }
  }

  if (responseRecv == false)
  {
    printDebug(DEBUG_WARNING, "STUN", DC_NEGOTIATING,
        "Failed to receive Nomination Response from remote!", {
          pair->remote->address, QString(pair->remote->port)
        }
    );

    if (multiplex_ == false)
    {
      udp_->unbind();
    }
    return false;
  }

  if (multiplex_ == false)
  {
    udp_->unbind();
  }
  return responseRecv;
}

bool Stun::sendNominationResponse(ICEPair *pair)
{
  Q_ASSERT(pair != nullptr);

  if (multiplex_ == false)
  {
    if (!udp_->bindRaw(QHostAddress(pair->local->address), pair->local->port))
    {
      printDebug(DEBUG_ERROR, "STUN", DC_NEGOTIATING,
          "Binding failed! Cannot send STUN Binding Requests to", {
            pair->remote->address, QString(pair->remote->port)
          }
      );
      return false;
    }

    connect(udp_, &UDPServer::rawMessageAvailable, this, &Stun::recvStunMessage);
  }

  STUNMessage msg;
  QByteArray message = QByteArray("hole punch nomination");
  bool nominationRecv = false;

  for (int i = 0; i < 128; ++i)
  {
    udp_->sendData(
        message,
        QHostAddress(pair->remote->address),
        pair->remote->port,
        false
    );

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
        udp_->sendData(
            message,
            QHostAddress(pair->remote->address),
            pair->remote->port,
            false
        );
        QThread::msleep(20);
      }

      break;
    }
  }

  if (nominationRecv == false)
  {
    printDebug(DEBUG_WARNING, "STUN", DC_NEGOTIATING,
        "Failed to receive Nomination Request from remote!", {
          pair->remote->address, QString(pair->remote->port)
        }
    );

    if (multiplex_ == false)
    {
      udp_->unbind();
    }
    return false;
  }

  if (multiplex_ == false)
  {
    udp_->unbind();
  }

  return nominationRecv;
}

void Stun::processReply(QByteArray data)
{
  if(data.size() < 20)
  {
    printDebug(DEBUG_WARNING, "STUN", DC_NEGOTIATING,
        "Received too small response to STUN query!");
    return;
  }

  QString message = QString::fromStdString(data.toHex().toStdString());

  STUNMessage response = stunmsg_.networkToHost(data);

  if (!stunmsg_.validateStunResponse(response))
  {
    printDebug(DEBUG_WARNING, "STUN", DC_NEGOTIATING, "Invalid STUN Response!");
    emit stunError();
    return;
  }

  std::pair<QHostAddress, uint16_t> addressInfo;

  if (response.getXorMappedAddress(addressInfo))
  {
    emit addressReceived(addressInfo.first);
  }
  else
  {
    printDebug(DEBUG_WARNING, "STUN", DC_NEGOTIATING, "DIDN'T GET XOR-MAPPED-ADDRESS!");
    emit stunError();
  }
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
  QByteArray data      = message.data();
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
    if (stunmsg_.validateStunResponse(
          stunMsg,
          message.senderAddress(),
          message.senderPort()))
    {
      emit parsingDone();
    }
  }
  else
  {
    /* TODO:  */
    /* printDebug(DEBUG_WARNING, "STUN", DC_NEGOTIATING, */
    /*     { "type", "from", "to" }, */
    /*     { stunMsg.getType(), QString(message.senderAddress()) + QString(message.senderPort()), */
    /*       QString(message.destinationAddress()) + QString(message.destinationPort())}); */
  }
}

