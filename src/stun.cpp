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
bool controller_ = false;
bool nominationOngoing = false;

STUNMessage generateRequest();
QByteArray toNetwork(STUNMessage request);
STUNMessage fromNetwork(QByteArray& data, char **outRawAddress);

Stun::Stun():
  udp_()
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

  STUNMessage request = generateRequest();
  for(unsigned int i = 0; i < 12; ++i)
  {
    transactionID_[i] = request.transactionID[i];
  }
  QByteArray message = toNetwork(request);

  udp_.sendData(message, address, GOOGLE_STUN_PORT, false);
}

STUNMessage generateRequest()
{
  STUNMessage request = {
    .type        = STUN_REQUEST,
    .length      = 0,
    .magicCookie = 0x2112A442
  };

  // TODO: crytographically random. see <random>
  // no real reason for seed. Just to make it less obvious for collisions
  qsrand(QDateTime::currentSecsSinceEpoch() / 2 + 1);

  for (unsigned int i = 0; i < 12; ++i)
  {
    request.transactionID[i] = qToBigEndian(uint8_t(qrand() % 256));
  }

  return request;
}

STUNMessage *Stun::generateResponse(size_t payloadSize, void *payload)
{
  STUNMessage *response = (STUNMessage *)malloc(sizeof(STUNMessage) + payloadSize);

  response->type        = STUN_RESPONSE;
  response->length      = payloadSize;
  response->magicCookie = 0x2112A442;

  for (unsigned int i = 0; i < 12; ++i)
  {
    response->transactionID[i] = transactionID_[i];
  }

  if (payload != NULL)
  {
    memcpy(response->payload, payload, payloadSize);
  }

  return response;
}

QByteArray toNetwork(STUNMessage request)
{
  request.type        = qToBigEndian((short)request.type);
  request.length      = qToBigEndian(request.length);
  request.magicCookie = qToBigEndian(request.magicCookie);

  for(unsigned int i = 0; i < 12; ++i)
  {
    request.transactionID[i] = qToBigEndian(request.transactionID[i]);
  }

  memcpy(&request.transactionID, &request.transactionID, sizeof(request.transactionID));
  char *data = (char *)malloc(sizeof(request)); // TODO memory leak here?
  memcpy(data, &request, sizeof(request));

  return QByteArray(data, sizeof(request));
}

QByteArray toNetwork(STUNMessage *response)
{
  unsigned int len      = response->length;

  response->type        = qToBigEndian((short)response->type);
  response->length      = qToBigEndian(response->length);
  response->magicCookie = qToBigEndian(response->magicCookie);

  for (unsigned int i = 0; i < 12; ++i)
  {
    response->transactionID[i] = qToBigEndian(response->transactionID[i]);
  }

  for (unsigned int i = 0; i < len; ++i)
  {
    response->payload[i] = qToBigEndian(response->payload[i]);
  }

  memcpy(response->transactionID, response->transactionID, sizeof(response->transactionID));
  memcpy(response->payload,       response->payload,       len);

  return QByteArray((char *)response, sizeof(STUNMessage) + len);
}

// TODO give attributes for stun message as parameter for this function
QByteArray Stun::generateMessage()
{
  STUNMessage request = generateRequest();

  for (unsigned int i = 0; i < 12; ++i)
  {
    transactionID_[i] = request.transactionID[i];
  }

  return toNetwork(request);
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

bool Stun::sendBindingRequest(QString addressRemote, int portRemote, QString addressLocal, int portLocal, bool  controller)
{
  controller_ = controller;
  qDebug() << "BINDING " << addressLocal << " TO PORT " << portLocal;

  if (!udp_.bindRaw(QHostAddress(addressLocal), portLocal))
  {
    qDebug() << "Binding failed! Cannot send STUN Binding Requests to " << addressRemote << ":" << portRemote;
    return false;
  }

  connect(&udp_, &UDPServer::rawMessageAvailable, this, &Stun::recvStunMessage);

  // TODO ICE_CONTROLLING jos controller == true, muuten ICE_CONTROLLED
  QByteArray message = generateMessage(); 

  bool ok = false;

  for (int i = 0; i < 10; ++i)
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

bool Stun::sendBindingResponse(QString addressRemote, int portRemote)
{
  STUNMessage *responseMsg = generateResponse(addressRemote.size() + sizeof(int), NULL);

  // TODO attribute
  uint16_t port    = ((uint16_t)port) ^ 0x2112;
  uint32_t address = ((uint32_t)(QHostAddress(addressRemote).toIPv4Address())) ^ responseMsg->magicCookie;

  memcpy(responseMsg->payload,                    &port,    sizeof(uint16_t));
  memcpy(responseMsg->payload + sizeof(uint16_t), &address, sizeof(uint32_t));

  QByteArray out = toNetwork(responseMsg);

  for (int i = 0; i < 10; ++i)
  {
    udp_.sendData(out, QHostAddress(addressRemote), portRemote, false);
  }

  free(responseMsg);
}

STUNMessage Stun::getStunMessage(QByteArray data)
{
  char *outRawAddress = nullptr;
  STUNMessage response = fromNetwork(data, &outRawAddress);

  return response;
}

bool Stun::validateStunMessage(QByteArray data, int type)
{
  if (data.size() < 20)
  {
    qDebug() << "Received too small response to STUN query!";
    return false;
  }

  QString message = QString::fromStdString(data.toHex().toStdString());
  /* qDebug() << "Got a STUN reply:" << message << "with size:" << data.size(); */

  STUNMessage response = getStunMessage(data);

  if(response.type != type)
  {
    qDebug() << "Unsuccessful STUN transaction!";
    return false;
  }

  if ((response.length == 0 && response.type != STUN_REQUEST) || response.magicCookie != 0x2112A442)
  {
    qDebug() << "Something went wrong with STUN transaction values. Length:"
             << response.length << "Magic cookie:" << response.magicCookie;
    return false;
  }

  for(unsigned int i = 0; i < 12; ++i)
  {
    if(transactionID_[i] != response.transactionID[i])
    {
      /* qDebug() << "The transaction ID is not the same as sent!" << "Error at byte:" << i */
      /*          << "sent:" << transactionID_[i] << "got:" << response.transactionID[i]; */
      /* return false; */
    }
  }

  /* qDebug() << "Valid STUN response. Decoding address"; */
  return true;
}

bool Stun::validateStunResponse(QByteArray data)
{
  return validateStunMessage(data, STUN_RESPONSE);
}

bool Stun::validateStunRequest(QByteArray data)
{
  return validateStunMessage(data, STUN_REQUEST);
}

// either we got Stun binding request -> send binding response
// or Stun binding response -> mark candidate as valid
void Stun::recvStunMessage(QNetworkDatagram message)
{
  QByteArray data     = message.data();
  STUNMessage stunMsg = getStunMessage(data);

  if (stunMsg.type == STUN_REQUEST)
  {
    if (!validateStunRequest(data))
    {
      return;
    }

    // TODO check if USE_CANDIATE attribute has been set and if so, send nomination response??
    // or can binding response be used??
    sendBindingResponse(message.senderAddress().toString(), message.senderPort());
  }
  else if (stunMsg.type == STUN_RESPONSE)
  {
    if (validateStunResponse(data))
    {
      emit parsingDone();
    }
  }
  else
  {
    qDebug() << "WARNING: Got unkown STUN message, type: " << stunMsg.type
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

  QByteArray outMessage = generateMessage();
  bool ok = false;

  for (int i = 0; i < 1000; ++i)
  {
    udp_.sendData(outMessage, QHostAddress(addressRemote), portRemote, false);

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

  QByteArray outMessage = generateMessage();
  bool ok = false;

  for (int i = 0; i < 50; ++i)
  {
    udp_.sendData(outMessage, QHostAddress(addressRemote), portRemote, false);

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

STUNMessage fromNetwork(QByteArray& data, char** outRawAddress)
{
  STUNMessage response;
  char *raw_data = data.data();

  response.type        = qFromBigEndian(*((uint16_t *)&raw_data[0]));
  response.length      = qFromBigEndian(*((uint16_t *)&raw_data[2]));
  response.magicCookie = qFromBigEndian(*((uint32_t *)&raw_data[4]));

  for (unsigned int i = 0; i < 12; ++i)
  {
    response.transactionID[i] = (uint8_t)raw_data[8 + i];
  }

  /* qDebug() << "Type:" << response.type */
  /*          << "Length:" << response.length */
  /*          << "Magic Cookie:" << response.magicCookie; */

  *outRawAddress = (char*)malloc(response.length);

  memcpy(*outRawAddress, (void*)&raw_data[20], response.length);

  return response;
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

  char* outRawAddress = nullptr;
  STUNMessage response = fromNetwork(data, &outRawAddress);

  if(response.type != 0x0101)
  {
    qDebug() << "Unsuccessful STUN transaction!";
    emit stunError();
    return;
  }

  if(response.length == 0 || response.magicCookie != 0x2112A442)
  {
    qDebug() << "Something went wrong with STUN transaction values. Length:"
             << response.length << "Magic cookie:" << response.magicCookie;
    emit stunError();
    return;
  }

  for(unsigned int i = 0; i < 12; ++i)
  {
    if(transactionID_[i] != response.transactionID[i])
    {
      qDebug() << "The transaction ID is not the same as sent!" << "Error at byte:" << i
               << "sent:" << transactionID_[i] << "got:" << response.transactionID[i];
    }
  }
  qDebug() << "Valid STUN response. Decoding address";

  QHostAddress address;

  // enough for attribute
  if(response.length >= 4)
  {
    uint16_t attr_type = qFromBigEndian(*((uint16_t*)&outRawAddress[0]));
    uint16_t attr_length = qFromBigEndian(*((uint16_t*)&outRawAddress[2]));

    if(attr_type == 0x0020)
    {
      if(attr_length >= 8)
      {
        // outRawAddress[4] is ignored according to RFC 5389
        uint8_t ip_type = outRawAddress[5];

        if(ip_type == 0x01)
        {
          if(attr_length == 8)
          {
            uint16_t port     = qFromBigEndian(*((uint16_t *)&outRawAddress[6])) ^ 0x2112;
            uint32_t uAddress = qFromBigEndian(*((uint32_t *)&outRawAddress[8])) ^ response.magicCookie;

            address.setAddress(uAddress);

            qDebug() << "Got our address from STUN:" << address.toString() << ":" << port;

            emit addressReceived(address);
            return; // success
          }
          else
          {
            qDebug() << "the XOR_MAPPED IPv4 attribution length is not 8:" << attr_length;
          }
        }
        else if(ip_type == 0x02)
        {
          // TODO: Add IPv6 support for STUN
          qDebug() << "Received IPv6 address from STUN. Support not implemented yet.";
        }
        else
        {
          qDebug() << "Some crap in STUN ipv field";
        }
      }
      else
      {
        qDebug() << "STUN XOR_MAPPED_ATTRIBUTE is too short:" << attr_length;
      }
    }
    else
    {
      qDebug() << "Unsupproted STUN attribute:" << attr_type;
    }
  }
  else
  {
    qDebug() << "Response length too short:" << response.length;
  }

  emit stunError();
}
