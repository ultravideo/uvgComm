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

  STUNMessage request;

  request.type = qToBigEndian((short)1);
  request.length = qToBigEndian(0x0000);
  request.magicCookie = qToBigEndian(0x2112A442);

  qsrand(QDateTime::currentSecsSinceEpoch()/2 + 1);

  generate96bits(request.transactionID);

  char * data = (char*)malloc(sizeof(request));
  memcpy(data, &request, sizeof(request));

  QByteArray ba = QByteArray(data,sizeof(request) );
  udp_.sendData(ba, address, GOOGLE_STUN_PORT, false);
}

uint32_t Stun::generate96bits(uint8_t transactionID[12])
{
  // TODO: crytographically random. see <random>
  for(unsigned int i = 0; i < 12; ++i)
  {
    transactionID[i] = qrand()%256;
  }

  qDebug() << "Generated STUN transactionID:" << transactionID;
}

void Stun::processReply(QByteArray data)
{
  QString message = QString::fromStdString(data.toHex().toStdString());
  qDebug() << "Got a STUN reply:" << message;


  // TODO XOR the address
 // emit addressReceived(message);
}

