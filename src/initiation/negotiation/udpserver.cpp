#include "common.h"
#include "udpserver.h"
#include <QUdpSocket>
#include <QMetaObject>
#include "stun.h"

UDPServer::UDPServer():
  socket_(nullptr),
  sendPort_(0)
{}

bool UDPServer::bindSocket(const QHostAddress& address, quint16 port, enum SOCKET_TYPE type)
{
  this->unbind();

  sendPort_ = port;
  socket_ = new QUdpSocket(this);

  QString addressDebug = address.toString() + ":" + QString::number(port);

  if(!socket_->bind(address, port))
  {
    printDebug(DEBUG_ERROR, "UDPServer", 
        "Failed to bind UDP Socket to", {"Address"},
          {addressDebug});
    return false;
  }
  else
  {
    //printNormal(this, "Binded UDP Port", {"Address"}, addressDebug);
  }

  switch (type)
  {
    case SOCKET_RAW:
      connect(socket_, &QUdpSocket::readyRead, this, &UDPServer::readDatagram);
      break;

    case SOCKET_MULTIPLEXED:
      connect(socket_, &QUdpSocket::readyRead, this, &UDPServer::readMultiplexData);
      break;
  }

  return true;
}

void UDPServer::unbind()
{
  sendPort_ = 0;

  if (socket_)
  {
    socket_->blockSignals(true);
    socket_->close();
    delete socket_;
    socket_ = nullptr;
  }
}

bool UDPServer::bind(const QHostAddress &address, quint16 port)
{
  return bindSocket(address, port, SOCKET_RAW);
}


bool UDPServer::bindMultiplexed(const QHostAddress& address, quint16 port)
{
  return bindSocket(address, port, SOCKET_MULTIPLEXED);
}

void UDPServer::sendData(QByteArray& data, const QHostAddress &local,
                         const QHostAddress& remote,
                         quint16 remotePort,
                         bool untilReply)
{

  if(data.size() > 512)
  {
    printDebug(DEBUG_WARNING, "UDPServer",
                "Sending too large UDP packet!");
   // TODO do something maybe
  }
  else if (data.size() == 0)
  {
    printDebug(DEBUG_WARNING, "UDPServer",
                "Trying to send an empty UDP packet!");
    return;
  }

  QNetworkDatagram datagram = QNetworkDatagram(data, remote, remotePort);
  datagram.setSender(local, (quint16)sendPort_);

  if (socket_->writeDatagram(datagram) < 0)
  {
    printWarning(this, "Failed to send UDP datagram!", {"Path"},
              {local.toString() + ":" + QString::number(sendPort_) + " -> " +
               remote.toString() + ":" + QString::number(remotePort)});
  }
  else
  {
    printNormal(this, "Successfully sent UDP datagram!", {"Path"},
              {local.toString() + ":" + QString::number(sendPort_) + " -> " +
               remote.toString() + ":" + QString::number(remotePort)});
  }
}


void UDPServer::readDatagram()
{
  while (socket_->hasPendingDatagrams())
  {
    emit datagramAvailable(socket_->receiveDatagram());
  }
}


void UDPServer::readMultiplexData()
{
  while (socket_->hasPendingDatagrams())
  {
    QNetworkDatagram datagram = socket_->receiveDatagram();

    // is anyone listening to  messages from this sender?
    if (listeners_.contains(datagram.senderAddress().toString()))
    {
      if (listeners_[datagram.senderAddress().toString()]
          .contains(datagram.senderPort()))
      {
        QMetaObject::invokeMethod(
            listeners_[datagram.senderAddress().toString()][datagram.senderPort()],
            "recvStunMessage",
            Qt::DirectConnection,
            Q_ARG(QNetworkDatagram, datagram)
        );
      }
    }
  }
}

void UDPServer::expectReplyFrom(Stun *stun, QString& address, quint16 port)
{
    listeners_[address][port] = stun;
}
