#include "stun.h"

#include <QDnsLookup>
#include <QHostInfo>
#include <QDateTime>
#include <QDebug>
#include <QMutex>
#include <QWaitCondition>
#include <QEventLoop>

#include <QtEndian>

const uint16_t GOOGLE_STUN_PORT = 19302;
const uint16_t STUN_PORT = 21000;
bool nominationOngoing = false;

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

  STUNMessage request_ = stunmsg_.createRequest();
  QByteArray   message_ = stunmsg_.hostToNetwork(request_);

  udp_.sendData(message_, address, GOOGLE_STUN_PORT, false);
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

bool Stun::sendBindingRequest(QString addressRemote, int portRemote, QString addressLocal, int portLocal, bool controller)
{
  if (controller)
    qDebug() << "[controller] BINDING " << addressLocal << " TO PORT " << portLocal;
  else
    qDebug() << "[controllee] BINDING " << addressLocal << " TO PORT " << portLocal;

  if (!udp_.bindRaw(QHostAddress(addressLocal), portLocal))
  {
    qDebug() << "Binding failed! Cannot send STUN Binding Requests to " << addressRemote << ":" << portRemote;
    return false;
  }

  connect(&udp_, &UDPServer::rawMessageAvailable, this, &Stun::recvStunMessage);

  STUNMessage request = stunmsg_.createRequest();
  request.addAttribute(controller ? STUN_ATTR_ICE_CONTROLLING : STUN_ATTR_ICE_CONTROLLED);
  request.addAttribute(STUN_ATTR_PRIORITY, 0x1337); // TODO how get priority from iceflowcontrol??

  QByteArray message = stunmsg_.hostToNetwork(request);

  bool ok = false;

  for (int i = 0; i < 50; ++i)
  {
    udp_.sendData(message, QHostAddress(addressRemote), portRemote, false);

    if (waitForStunResponse(20 * (i + 1)))
    {
      qDebug() << "got stun response!";
      ok = true;
      break;
    }
  }

  if (waitForStunResponse(100))
  {
    ok = true;
  }

  udp_.unbind();
  return ok;
}

// TODO anna tälle parametriksi STUNMesage ja kopioi transactionID sieltä!
bool Stun::sendBindingResponse(QString addressRemote, int portRemote)
{
  STUNMessage response = stunmsg_.createResponse();
  response.addAttribute(STUN_ATTR_ICE_CONTROLLED); // TODO find out whether we're controlling or being controlled
  response.addAttribute(STUN_ATTR_PRIORITY, 0x1338);

  QByteArray message = stunmsg_.hostToNetwork(response);

  for (int i = 0; i < 5; ++i)
  {
    udp_.sendData(message, QHostAddress(addressRemote), portRemote, false);
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
      if (stunMsg.hasAttribute(STUN_ATTR_USE_CANDIATE))
      {
        /* qDebug() << "MESSAGE HAS ATTRIBUTE USE_CANDIATE!"; */
        emit nominationRecv();
      }
      else
      {
        sendBindingResponse(message.senderAddress().toString(), message.senderPort());
      }
    }
  }
  else if (stunMsg.getType() == STUN_RESPONSE)
  {
    if (stunmsg_.validateStunResponse(stunMsg))
    {
      emit parsingDone();
    }
  }
  else
  {
    qDebug() << "WARNING: Got unkown STUN message, type: "      << stunMsg.getType()
             << " from " << message.senderAddress()      << ":" << message.senderPort()
             << " to"    << message.destinationAddress() << ":" << message.destinationPort();
    qDebug() << "\n\n\n\n";
    while (1);
  }
}

bool Stun::sendNominationRequest(QString addressRemote, int portRemote, QString addressLocal, int portLocal)
{
  nominationOngoing = true;

  qDebug() << "[controller] BINDING " << addressLocal << " TO PORT " << portLocal;

  if (!udp_.bindRaw(QHostAddress(addressLocal), portLocal))
  {
    qDebug() << "Binding failed! Cannot send STUN Binding Requests to " << addressRemote << ":" << portRemote;
    return false;
  }

  connect(&udp_, &UDPServer::rawMessageAvailable, this, &Stun::recvStunMessage);

  STUNMessage request = stunmsg_.createRequest();
  request.addAttribute(STUN_ATTR_ICE_CONTROLLING);
  request.addAttribute(STUN_ATTR_USE_CANDIATE);

  QByteArray message  = stunmsg_.hostToNetwork(request);

  bool ok = false;

  for (int i = 0; i < 25; ++i)
  {
    udp_.sendData(message, QHostAddress(addressRemote), portRemote, false);

    if (waitForStunResponse(20 * (i + 1)))
    {
      ok = true;
      break;
    }
  }

  udp_.unbind();
  return ok;
}

bool Stun::sendNominationResponse(QString addressRemote, int portRemote, QString addressLocal, int portLocal)
{
  nominationOngoing = true;
  qDebug() << "[controllee] BINDING " << addressLocal << " TO PORT " << portLocal;

  if (!udp_.bindRaw(QHostAddress(addressLocal), portLocal))
  {
    qDebug() << "Binding failed! Cannot send STUN Binding Requests to " << addressRemote << ":" << portRemote;
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
    udp_.sendData(reqMessage, QHostAddress(addressRemote), portRemote, false);

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
      udp_.sendData(respMessage, QHostAddress(addressRemote), portRemote, false);

      // when we no longer get nomination request it means that remote has either received
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

  if (response.getType() != STUN_RESPONSE)
  {
    qDebug() << "Unsuccessful STUN transaction!";
    emit stunError();
    return;
  }

  if (response.getLength() == 0 || response.getCookie() != STUN_MAGIC_COOKIE)
  {
    qDebug() << "Something went wrong with STUN transaction values. Length:"
             << response.getLength() << "Magic cookie:" << response.getCookie();
    emit stunError();
    return;
  }

  if (!stunmsg_.verifyTransactionID(response))
  {
    qDebug() << "TransactionID is invalid!";
  }

  qDebug() << "Valid STUN response. Decoding address";

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
