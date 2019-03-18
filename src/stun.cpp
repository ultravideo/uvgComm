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

bool Stun::sendBindingRequest(QString addressRemote, int portRemote, QString addressLocal, int portLocal, bool controller)
{
  qDebug() << "BINDING " << addressLocal << " TO PORT " << portLocal;

  if (!udp_.bindRaw(QHostAddress(addressLocal), portLocal))
  {
    qDebug() << "Binding failed! Cannot send STUN Binding Requests to " << addressRemote << ":" << portRemote;
    return false;
  }

  connect(&udp_, &UDPServer::rawMessageAvailable, this, &Stun::recvStunMessage);

  STUNMessage request = stunmsg_.createRequest();
  /* request.addAttribute(STUN_ATTR_PRIORITY); */
  /* request.addAttribute(controller ? STUN_ATTR_ICE_CONTROLLING : STUN_ATTR_ICE_CONTROLLED); */

  QByteArray message = stunmsg_.hostToNetwork(request);

  bool ok = false;

  for (int i = 0; i < 250; ++i)
  {
    udp_.sendData(message, QHostAddress(addressRemote), portRemote, false);

    if (waitForStunResponse(20))
    {
      ok = true;
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
  /* response.setLength(1); */

  /* response.addAttribute(STUN_ATTR_PRIORITY); */

  // TODO find out whether we're controlling or being controlled
  /* response.addAttribute(STUN_ATTR_ICE_CONTROLLED); */

  // TODO add addressRemote and portRemote to payload (xor-mapped-address)

  QByteArray message = stunmsg_.hostToNetwork(response);

  for (int i = 0; i < 15; ++i)
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
      // TODO check if USE_CANDIATE attribute has been set and if so, send nomination response??
      // or can binding response be used??
      sendBindingResponse(message.senderAddress().toString(), message.senderPort());
    }
  }
  else if (stunMsg.getType() == STUN_RESPONSE)
  {
    if (stunmsg_.validateStunResponse(stunMsg))
    {
      /* qDebug() << "negot. done for " << message.senderAddress().toString() << ":" << message.senderPort(); */
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

  // TODO construct normal requets but set USE-CANDIDATE attribute to indicate that this candidate has been nominated
  if (!udp_.bindRaw(QHostAddress(addressLocal), portLocal))
  {
    qDebug() << "Binding failed! Cannot send STUN Binding Requests to " << addressRemote << ":" << portRemote;
    return false;
  }

  connect(&udp_, &UDPServer::rawMessageAvailable, this, &Stun::recvStunMessage);

  STUNMessage request = stunmsg_.createRequest();

  // TODO
  /* request.addAttribute(STUN_ATTR_USE_CANDIATE); */
  /* request.addAttribute(STUN_ATTR_ICE_CONTROLLING); */
  /* request.addAttribute(STUN_ATTR_PRIORITY); */

  QByteArray message = stunmsg_.hostToNetwork(request);

  /* QByteArray outMessage = generateMessage(); */
  bool ok = false;

  for (int i = 0; i < 1000; ++i)
  {
    udp_.sendData(message, QHostAddress(addressRemote), portRemote, false);

    if (waitForStunResponse(20))
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

  /* STUNMessage response = stunmsg_.createResponse(); */
  STUNMessage response = stunmsg_.createRequest();
  /* response.addAttribute(STUN_ATTR_USE_CANDIATE); */

  QByteArray message = stunmsg_.hostToNetwork(response);
  bool ok = false;

  for (int i = 0; i < 50; ++i)
  {
    udp_.sendData(message, QHostAddress(addressRemote), portRemote, false);

    if (waitForStunResponse(50))
    {
      ok = true;
      continue;
    }
  }

  // we must release the port because media may be streamed through this ip/port
  udp_.unbind();
  return ok;
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
