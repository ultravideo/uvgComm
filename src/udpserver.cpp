#include "udpserver.h"

#include <QUdpSocket>
#include <QNetworkDatagram>



UDPServer::UDPServer():
  socket_(NULL),
  sendPort_(0)
{}

void UDPServer::bind(const QHostAddress &address, quint16 port)
{
  socket_ = new QUdpSocket(this);
  if(!socket_->bind(address, port))
  {
    qDebug() << "Failed to bind UDP socket to " << address.toString();
  }

  connect(socket_, SIGNAL(readyRead()),
          this, SLOT(readData()));

  sendPort_ = port;
}

void UDPServer::sendData(QByteArray& data, const QHostAddress &address, quint16 port, bool untilReply)
{
  qDebug() << "Sending the following UDP data:" << QString::fromStdString(data.toHex().toStdString())
           << "with size:" << data.size();
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
  QNetworkDatagram datagram = QNetworkDatagram (data, address, port);
  datagram.setSender(QHostAddress::AnyIPv4, (quint16)sendPort_);

  qDebug() << "Sending UDP packet to:" << address.toString() << ":" << port
           << "from port:" << datagram.senderPort();

  socket_->writeDatagram(datagram);
}

void UDPServer::readData()
{
  while (socket_->hasPendingDatagrams()) {
      QNetworkDatagram datagram = socket_->receiveDatagram();
      messageAvailable(datagram.data());
  }
}
