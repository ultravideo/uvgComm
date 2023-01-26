#include "udpserver.h"

#include "common.h"

#include <QUdpSocket>
#include <QMetaObject>

#include "logger.h"

UDPServer::UDPServer():
  socket_(nullptr),
  sendPort_(0)
{}

bool UDPServer::bindSocket(const QHostAddress& address, quint16 port)
{
  this->unbind();

  sendPort_ = port;
  socket_ = new QUdpSocket(this);

  QString addressDebug = address.toString() + ":" + QString::number(port);
  if(!socket_->bind(address, port))
  {
    Logger::getLogger()->printError(this, "Failed to bind UDP Socket to", 
                                    {"Interface"}, {addressDebug});
    return false;
  }
  else
  {
    //Logger::getLogger()->printNormal(this, "Binded UDP Port", {"Interface"}, 
    //                                  addressDebug);
  }

  connect(socket_, &QUdpSocket::readyRead, this, &UDPServer::readDatagram);

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
    Logger::getLogger()->printProgramError(this, "Sending UDP packet with invalid size. "
                                                 "Acceptable: 1 - 512",
                                           {"Size"}, {QString::number(data.size())});
    return false;
  }

  QNetworkDatagram datagram = QNetworkDatagram(data, remote, remotePort);
  datagram.setSender(local, (quint16)sendPort_);

  if (socket_->writeDatagram(datagram) < 0)
  {
    //Logger::getLogger()->printWarning(this, "Failed to send UDP datagram!", {"Path"},
    //                                  {local.toString() + ":" + QString::number(sendPort_) + " -> " +
    //                                   remote.toString() + ":" + QString::number(remotePort)});
    return false;
  }

  return true;
}


void UDPServer::readDatagram()
{
  while (socket_ && socket_->hasPendingDatagrams())
  {
    emit datagramAvailable(socket_->receiveDatagram());
  }
}

