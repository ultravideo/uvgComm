#include "stun.h"

#include <QDnsLookup>
#include <QHostInfo>
#include <QDateTime>
#include <QDebug>

#include <QtEndian>

const uint16_t GOOGLE_STUN_PORT = 19302;
const uint16_t STUN_PORT = 21000;

struct STUNMessage
{
  uint16_t type;
  uint16_t length;
  uint32_t magicCookie;
  uint8_t transactionID[12];
};

STUNMessage generateRequest();
QByteArray toNetwork(STUNMessage request);
STUNMessage fromNetwork(QByteArray& data, char **outRawAddress);

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

STUNMessage fromNetwork(QByteArray& data, char** outRawAddress)
{
  char *raw_data = data.data();
  STUNMessage response;
  response.type = qFromBigEndian(*((uint16_t*)&raw_data[0]));
  response.length = qFromBigEndian(*((uint16_t*)&raw_data[2]));
  response.magicCookie = qFromBigEndian(*((uint32_t*)&raw_data[4]));

  for(unsigned int i = 0; i < 12; ++i)
  {
    response.transactionID[i] = (uint8_t)raw_data[8 + i];
  }

  qDebug() << "Type:" << response.type
           << "Length:" << response.length
           << "Magic Cookie:" << response.magicCookie;

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

  char* outRawAddress = NULL;
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
            uint16_t port = qFromBigEndian(*((uint16_t*)&outRawAddress[6]))^0x2112;
            uint32_t uAddress = qFromBigEndian(*((uint32_t*)&outRawAddress[8]))^response.magicCookie;

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
