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
  udp_(),
  stunmsg_()
{}

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
    qDebug() <<"Found" << addresses.size() << "stun server addresses. Using:" << address.toString();
  }
  else
  {
    qDebug() << "Did not find any addresses";
    return;
  }

  udp_.bind(QHostAddress::AnyIPv4, STUN_PORT);
  QObject::connect(&udp_, SIGNAL(messageAvailable(QByteArray)), this, SLOT(processReply(QByteArray)));

  STUNMessage request = stunmsg_.createRequest();
  QByteArray message  = stunmsg_.hostToNetwork(request);

  stunmsg_.cacheRequest(request);

  udp_.sendData(message, address, GOOGLE_STUN_PORT, false);
}

bool Stun::waitForStunResponse(unsigned long timeout)
{
  QTimer timer;
  timer.setSingleShot(true);
  QEventLoop loop;

  connect(this,   &Stun::parsingDone, &loop, &QEventLoop::quit);
  connect(&timer, &QTimer::timeout,   &loop, &QEventLoop::quit);

  timer.start(timeout);
  loop.exec();

  return timer.isActive();
}

bool Stun::waitForStunRequest(unsigned long timeout)
{
  QTimer timer;
  timer.setSingleShot(true);
  QEventLoop loop;

  connect(this,   &Stun::requestRecv, &loop, &QEventLoop::quit);
  connect(&timer, &QTimer::timeout,   &loop, &QEventLoop::quit);

  timer.start(timeout);
  loop.exec();

  return timer.isActive();
}

bool Stun::waitForNominationRequest(unsigned long timeout)
{
  QTimer timer;
  timer.setSingleShot(true);
  QEventLoop loop;

  connect(this,   &Stun::nominationRecv, &loop, &QEventLoop::quit);
  connect(&timer, &QTimer::timeout,   &loop, &QEventLoop::quit);

  timer.start(timeout);
  loop.exec();

  return timer.isActive();
}

// if we're the controlling agent, sending binding request is rather straight-forward:
// send request, wait for response and return
bool Stun::controllerSendBindingRequest(ICEPair *pair)
{
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
    udp_.sendData(message, QHostAddress(pair->remote->address), pair->remote->port, false);

    if (waitForStunResponse(20 * (i + 1)))
    {
      qDebug() << "GOT RESPONSE!";
      msgReceived = true;
      break;
    }
  }

  if (msgReceived == false)
  {
    qDebug() << "WARNING: Failed to receive STUN Binding Response from remote!";
    qDebug() << "Remote: " << pair->remote->address << ":" << pair->remote->port;
    udp_.unbind();
    return false;
  }

  // the first part of connectivity check is done, now we must wait for
  // remote's binding request and responed to them
  msgReceived = false;

  for (int i = 0; i < 20; ++i)
  {
    if (waitForStunRequest(20 * (i + 1)))
    {
      qDebug() << "[CONTROLLER] GOT STUN REQUEST!";

      msg = stunmsg_.createResponse();
      msg.addAttribute(STUN_ATTR_ICE_CONTROLLING);

      message     = stunmsg_.hostToNetwork(msg);
      msgReceived = true;

      for (int i = 0; i < 5; ++i)
      {
        udp_.sendData(message, QHostAddress(pair->remote->address), pair->remote->port, false);
        QThread::msleep(20);
      }

      break;
    }
  }

  udp_.unbind();
  return msgReceived;
}

bool Stun::controlleeSendBindingRequest(ICEPair *pair)
{
  Q_ASSERT(pair != nullptr);

  bool msgReceived   = false;
  QByteArray message = QByteArray("hole punch");
  STUNMessage msg;

  for (int i = 0; i < 20; ++i)
  {
    udp_.sendData(message, QHostAddress(pair->remote->address), pair->remote->port, false);

    if (waitForStunRequest(20 * (i + 1)))
    {
      msg = stunmsg_.createResponse();
      msg.addAttribute(STUN_ATTR_ICE_CONTROLLED);

      message     = stunmsg_.hostToNetwork(msg);
      msgReceived = true;

      for (int i = 0; i < 3; ++i)
      {
        udp_.sendData(message, QHostAddress(pair->remote->address), pair->remote->port, false);
        QThread::msleep(20);
      }

      break;
    }
  }

  if (msgReceived == false)
  {
    qDebug() << "WARNING: Failed to receive STUN Binding Request from remote!";
    qDebug() << "Remote: " << pair->remote->address << ":" << pair->remote->port;
    udp_.unbind();
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
    udp_.sendData(message, QHostAddress(pair->remote->address), pair->remote->port, false);

    if (waitForStunResponse(20 * (i + 1)))
    {
      msgReceived = true;
      break;
    }
  }

  udp_.unbind();
  return msgReceived;
}

bool Stun::sendBindingRequest(ICEPair *pair, bool controller)
{
  if (controller)
    qDebug() << "[controller] BINDING " << pair->local->address<< " TO PORT " << pair->local->port;
  else
    qDebug() << "[controllee] BINDING " << pair->local->address << " TO PORT " << pair->local->port;

  if (!udp_.bindRaw(QHostAddress(pair->local->address), pair->local->port))
  {
    qDebug() << "Binding failed! Cannot send STUN Binding Requests to " 
             << pair->remote->address << ":" << pair->remote->port;
    return false;
  }

  connect(&udp_, &UDPServer::rawMessageAvailable, this, &Stun::recvStunMessage);

  if (controller)
  {
    return controllerSendBindingRequest(pair);
  }

  return controlleeSendBindingRequest(pair);
}

bool Stun::sendBindingResponse(STUNMessage& request, QString addressRemote, int portRemote)
{
  STUNMessage response = stunmsg_.createResponse(request);

  response.addAttribute(STUN_ATTR_PRIORITY, 1337);

  response.addAttribute(
      request.hasAttribute(STUN_ATTR_ICE_CONTROLLED)
      ? STUN_ATTR_ICE_CONTROLLING
      : STUN_ATTR_ICE_CONTROLLED
  );

  QByteArray message = stunmsg_.hostToNetwork(response);

  for (int i = 0; i < 25; ++i)
  {
    udp_.sendData(message, QHostAddress(addressRemote), portRemote, false);

    if (!waitForStunRequest(20 * (i + 1)))
    {
      break;
    }
  }
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
    // if this message is a response to a request we just sent, its TransactionID should be cached
    // if not, vertaa viimeksi lähetetyn TransactionID:tä vasten
    if (stunmsg_.validateStunResponse(stunMsg, message.senderAddress(), message.senderPort()))
    {
      emit parsingDone();
    }
  }
  else
  {
    qDebug() << "Got unknown STUN message, type: "      << stunMsg.getType()
             << " from " << message.senderAddress()      << ":" << message.senderPort()
             << " to"    << message.destinationAddress() << ":" << message.destinationPort();
  }
}

bool Stun::sendNominationRequest(ICEPair *pair)
{
  qDebug() << "[controller] BINDING " << pair->local->address << " TO PORT " << pair->local->port;

  if (!udp_.bindRaw(QHostAddress(pair->local->address), pair->local->port))
  {
    qDebug() << "Binding failed! Cannot send STUN Binding Requests to " << pair->remote->address << ":" << pair->remote->address;
    return false;
  }

  connect(&udp_, &UDPServer::rawMessageAvailable, this, &Stun::recvStunMessage);

  STUNMessage request = stunmsg_.createRequest();
  request.addAttribute(STUN_ATTR_ICE_CONTROLLING);
  request.addAttribute(STUN_ATTR_USE_CANDIATE);

  // expect reply for this message from remote
  stunmsg_.expectReplyFrom(request, pair->remote->address, pair->remote->port);

  QByteArray message  = stunmsg_.hostToNetwork(request);

  bool ok = false;

  for (int i = 0; i < 25; ++i)
  {
    udp_.sendData(message, QHostAddress(pair->remote->address), pair->remote->port, false);

    if (waitForStunResponse(20 * (i + 1)))
    {
      ok = true;
      break;
    }
  }

  udp_.unbind();
  return ok;
}

bool Stun::sendNominationResponse(ICEPair *pair)
{
  qDebug() << "[controllee] BINDING " << pair->local->address << " TO PORT " << pair->local->port;

  if (!udp_.bindRaw(QHostAddress(pair->local->address), pair->local->port))
  {
    qDebug() << "Binding failed! Cannot send STUN Binding Requests to " << pair->remote->address << ":" << pair->remote->address;
    return false;
  }

  connect(&udp_, &UDPServer::rawMessageAvailable, this, &Stun::recvStunMessage);

  // first send empty stun binding requests to remote to create a hole in the firewall
  // when the first nomination request is received (if any), start sending nomination response
  STUNMessage request = stunmsg_.createRequest();
  request.addAttribute(STUN_ATTR_ICE_CONTROLLED);

  QByteArray reqMessage = stunmsg_.hostToNetwork(request);
  bool nominationRecv   = false;

  for (int i = 0; i < 25; ++i)
  {
    udp_.sendData(reqMessage, QHostAddress(pair->remote->address), pair->remote->port, false);

    if (waitForNominationRequest(20 * (i + 1)))
    {
      nominationRecv = true;
      break;
    }
  }

  if (nominationRecv)
  {
    nominationRecv = false;

    STUNMessage response  = stunmsg_.createResponse();
    response.addAttribute(STUN_ATTR_ICE_CONTROLLED);
    response.addAttribute(STUN_ATTR_USE_CANDIATE);

    QByteArray respMessage = stunmsg_.hostToNetwork(response);

    for (int i = 0; i < 25; ++i)
    {
      udp_.sendData(respMessage, QHostAddress(pair->remote->address), pair->remote->port, false);

      // when we no longer get nomination request it means that remote has received
      // our response message and end the nomination process
      //
      // We can stop sending nomination responses when the waitForNominationRequest() timeouts
      //
      // Because we got here (we received nomination request in the previous step) we can assume
      // that the connection works in both ends and waitForNominationRequest() doesn't just timeout
      // because it doesn't receive anything
      if (!waitForNominationRequest(20 * (i + 1)))
      {
        nominationRecv = true;
        break;
      }
    }
  }

  udp_.unbind();
  return nominationRecv;
}

void Stun::processReply(QByteArray data)
{
  if(data.size() < 20)
  {
    qDebug() << "Received too small response to STUN query!";
    return;
  }

  QString message = QString::fromStdString(data.toHex().toStdString());
  qDebug() << "Got a STUN reply:" << message << "with size:" << data.size();

  STUNMessage response = stunmsg_.networkToHost(data);

  if (!stunmsg_.validateStunResponse(response))
  {
    qDebug() << "Invalid STUN Response!";
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
    qDebug() << "DIDN'T GET XOR-MAPPED-ADDRESS!";
    emit stunError();
  }
}
