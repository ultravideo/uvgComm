#include "udpserver.h"
#include <QUdpSocket>

UDPServer::UDPServer():
  socket_(nullptr),
  sendPort_(0)
{}

bool UDPServer::bindSocket(const QHostAddress& address, quint16 port, bool raw)
{
  this->unbind();

  sendPort_ = port;
  socket_ = new QUdpSocket(this);

  if(!socket_->bind(address, port))
  {
    qDebug() << "Failed to bind UDP socket to " << address.toString();
    qDebug() << socket_->error();
    return false;
  }

  if (raw)
  {
    connect(socket_, &QUdpSocket::readyRead, this, &UDPServer::readRawData);
  }
  else
  {
    connect(socket_, &QUdpSocket::readyRead, this, &UDPServer::readData);
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

void UDPServer::bind(const QHostAddress &address, quint16 port)
{
  (void)bindSocket(address, port, false);
}

bool UDPServer::bindRaw(const QHostAddress &address, quint16 port)
{
  return bindSocket(address, port, true);
}

void UDPServer::sendData(QByteArray& data, const QHostAddress &address, quint16 port, bool untilReply)
{
  /* qDebug() << "Sending the following UDP data:" << QString::fromStdString(data.toHex().toStdString()) */
  /*          << "with size:" << data.size(); */

  if(data.size() > 512)
  {
    qWarning() << "Sending too large UDP packet!";
   // TODO do something maybe
  }
  else if (data.size() == 0)
  {
    qWarning() << "WARNING: Trying to send an empty UDP packet!";
    return;
  }

  QNetworkDatagram datagram = QNetworkDatagram(data, address, port);
  datagram.setSender(QHostAddress::AnyIPv4, (quint16)sendPort_);

  /* qDebug() << "Sending UDP packet to:" << address.toString() << ":" << port */
  /*          << "from port:" << datagram.senderPort(); */

  if (socket_->writeDatagram(datagram) < 0)
  {
    qDebug() << "FAILED TO SEND DATA!";
  }
}

void UDPServer::readData()
{
  while (socket_->hasPendingDatagrams())
  {
    QNetworkDatagram datagram = socket_->receiveDatagram();
    emit messageAvailable(datagram.data());
  }
}

void UDPServer::readRawData()
{
  while (socket_->hasPendingDatagrams())
  {
    emit rawMessageAvailable(socket_->receiveDatagram());
  }
}
