#include "common.h"
#include "udpserver.h"
#include <QUdpSocket>
#include <QMetaObject>
#include "stun.h"

UDPServer::UDPServer():
  socket_(nullptr),
  sendPort_(0)
{}

bool UDPServer::bindSocket(const QHostAddress& address, quint16 port, bool multiplexed)
{
  this->unbind();

  sendPort_ = port;
  socket_ = new QUdpSocket(this);

  QString addressDebug = address.toString() + ":" + QString::number(port);
  if(!socket_->bind(address, port))
  {
    printError(this, "Failed to bind UDP Socket to", {"Interface"}, {addressDebug});
    return false;
  }
  else
  {
    printNormal(this, "Binded UDP Port", {"Interface"}, addressDebug);
  }

  if (!multiplexed)
  {
    connect(socket_, &QUdpSocket::readyRead, this, &UDPServer::readDatagram);
  }
  else
  {
    connect(socket_, &QUdpSocket::readyRead, this, &UDPServer::readMultiplexData);
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


bool UDPServer::sendData(QByteArray& data, const QHostAddress &local,
                         const QHostAddress& remote,
                         quint16 remotePort)
{
  if(data.size() > 512 || data.size() == 0)
  {
    printProgramError(this, "Sending UDP packet with invalid size. Acceptable: 1 - 512",
            {"Size"}, {QString::number(data.size())});
    return false;
  }

  QNetworkDatagram datagram = QNetworkDatagram(data, remote, remotePort);
  datagram.setSender(local, (quint16)sendPort_);

  if (socket_->writeDatagram(datagram) < 0)
  {
    printWarning(this, "Failed to send UDP datagram!", {"Path"},
              {local.toString() + ":" + QString::number(sendPort_) + " -> " +
               remote.toString() + ":" + QString::number(remotePort)});
    return false;
  }

  return true;
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

    // is anyone listening to messages from this sender?
    if (listeners_.contains(datagram.senderAddress().toString()) &&
        listeners_[datagram.senderAddress().toString()].contains(datagram.senderPort()))
    {
      QMetaObject::invokeMethod(
          listeners_[datagram.senderAddress().toString()][datagram.senderPort()],
          "recvStunMessage",
          Qt::DirectConnection,
          Q_ARG(QNetworkDatagram, datagram)
      );
    }
    else
    {
      printError(this, "Could not find listener for data", {"Address"}, {
                   datagram.senderAddress().toString() + "<-" + QString::number(datagram.senderPort())});
    }
  }
}


// TODO: This function creates a cross-dependency between UDPServer and Stun.
// This is not recommended. This class should not know anything about Stun.
void UDPServer::expectReplyFrom(Stun *stun, QString& address, quint16 port)
{
    listeners_[address][port] = stun;
}
