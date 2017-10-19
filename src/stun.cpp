#include "stun.h"

#include <QDnsLookup>
#include <QHostInfo>
#include <QDateTime>
#include <QDebug>

#include <QtEndian>

const uint16_t GOOGLE_STUN_PORT = 19302;
const uint16_t STUN_PORT = 3478;

struct STUNMessage
{
  uint16_t type;
  uint16_t length;
  uint32_t magicCookie;
  uint8_t transactionID[12];
};

STUNMessage generateRequest();
QByteArray toNetwork(STUNMessage request);
STUNMessage fromNetwork(QByteArray& data);

Stun::Stun():
  udp_()
{}

void Stun::wantAddress()
{
  // To find the IP address of qt-project.org
  QHostInfo::lookupHost("stun.l.google.com",
                        this, SLOT(handleHostaddress(QHostInfo)));
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
  STUNMessage request;
  request.type = 1;
  request.length = 0;
  request.magicCookie = 0x2112A442;

  // TODO: crytographically random. see <random>
  // no real reason for seed. Just to make it less obvious for collisions
  qsrand(QDateTime::currentSecsSinceEpoch()/2 + 1);
  for(unsigned int i = 0; i < 12; ++i)
  {
    request.transactionID[i] = qToBigEndian(uint8_t(qrand()%256));
  }

  qDebug() << "Generated STUN transactionID:" << request.transactionID;
  return request;
}

QByteArray toNetwork(STUNMessage request)
{
  request.type = qToBigEndian((short)request.type);
  request.length = qToBigEndian(request.length);
  request.magicCookie = qToBigEndian(request.magicCookie);

  for(unsigned int i = 0; i < 12; ++i)
  {
    request.transactionID[i] = qToBigEndian(request.transactionID[i]);
  }

  memcpy(&request.transactionID, &request.transactionID, sizeof(request.transactionID));
  char * data = (char*)malloc(sizeof(request));
  memcpy(data, &request, sizeof(request));

  return QByteArray(data,sizeof(request));
}

STUNMessage fromNetwork(QByteArray& data)
{
  const char * raw_data = data.constData();
  STUNMessage response;
  response.type = qFromBigEndian(*((short*)&raw_data[0]));
  response.length = qFromBigEndian(*((short*)&raw_data[2]));
  response.magicCookie = qFromBigEndian(*((int*)&raw_data[4]));

  for(unsigned int i = 0; i < 12; ++i)
  {
    response.transactionID[i] = (uint8_t)raw_data[8 + i];
  }

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
  qDebug() << "Got a STUN reply:" << message;

  STUNMessage response = fromNetwork(data);

  qDebug() << "Type:" << response.type
           << "Length:" << response.length
           << "Magic Cookie:" << response.magicCookie
           << "TransactionID:" << response.transactionID;

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
  qDebug() << "Successful STUN transaction!";


  // TODO XOR the address
 // emit addressReceived(message);
}



