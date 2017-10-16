#include "udpserver.h"

#include <QUdpSocket>
#include <QNetworkDatagram>

UDPServer::UDPServer():
  socket_(NULL)
{}

void UDPServer::bind(const QHostAddress &address, quint16 port)
{
  socket_ = new QUdpSocket(this);
  socket_->bind(address, port);

  connect(socket_, SIGNAL(readyRead()),
          this, SLOT(readData()));

  peerAddress_ = address;
  port_ = port;
}

void UDPServer::sendData(QString data, bool untilReply)
{
  QByteArray ba = data.toLatin1();
  // TODO check if larger than 512 bytes.
  socket_->writeDatagram(ba.data(), ba.size(), peerAddress_, port_);
}

void UDPServer::readData()
{
  while (socket_->hasPendingDatagrams()) {
      QNetworkDatagram datagram = socket_->receiveDatagram();
      QString message = QString::fromStdString(datagram.data().toStdString());
      messageAvailable(message);
  }
}
