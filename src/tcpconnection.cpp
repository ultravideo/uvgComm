#include "tcpconnection.h"

#include <QDataStream>
#include <QtConcurrent/QtConcurrent>

#include <stdint.h>

const uint32_t TOOMANYPACKETS = 100000;

TCPConnection::TCPConnection()
  :
    socket_(nullptr),
    shouldConnect_(false),
    connected_(),
    destination_(),
    port_(0),
    socketDescriptor_(0),
    buffer_(),
    sendMutex_(),
    running_(false),
    started_(false)
{
  qDebug() << "Constructing TCP connection";
}

void TCPConnection::init()
{
  QObject::connect(this, &TCPConnection::error, this, &TCPConnection::printError);
  running_ = true;

  start();
}

void TCPConnection::establishConnection(const QString &destination, uint16_t port)
{
  qDebug() << "Establishing connection to" << destination << ":" << port;

  destination_ = destination;
  port_ = port;
  shouldConnect_ = true;
  init();
}

void TCPConnection::setExistingConnection(qintptr socketDescriptor)
{
  qDebug() << "Setting existing/incoming connection. SocketDesc:" << socketDescriptor;
  socketDescriptor_ = socketDescriptor;
  shouldConnect_ = true;
  init();
}

void TCPConnection::sendPacket(const QString &data)
{
  qDebug() << "adding packet for sending";
  if(running_)
  {
    sendMutex_.lock();
    buffer_.push(data);
    sendMutex_.unlock();

    if(started_)
      eventDispatcher()->wakeUp();
  }
  else
  {
    qWarning() << "Warning: Not sending message, because sender has been shut down.";
  }
}

void TCPConnection::receivedMessage()
{
  qDebug() << "Socket ready to read";

  if(started_)
    eventDispatcher()->wakeUp();
}

void TCPConnection::connectLoop()
{
  const int connectionTimeout = 2 * 1000; // 2 seconds

  // TODO: close the connection just in case?

  if(socketDescriptor_ != 0)
  {
    qDebug() << "Getting socket:" << socketDescriptor_;
    if(!socket_->setSocketDescriptor(socketDescriptor_))
    {
      qCritical() << "ERROR: Could not get socket descriptor";
    }
  }
  else
  {
    qDebug() << "Connecting to address:" << destination_ << "Port:" << port_;
    socket_->connectToHost(destination_, port_);
  }

  if (socket_->state() != QAbstractSocket::ConnectedState &&
      !socket_->waitForConnected(connectionTimeout)) {
      emit error(socket_->error(), socket_->errorString());
      return;
  }
  connected_ = true;
  qDebug().nospace() << "Socket connected to our address: "
                     << socket_->localAddress().toString() << ":" << socket_->peerPort()
           << " Remote address: " << socket_->peerAddress().toString()
           << ":" << socket_->peerPort();

  emit socketConnected(socket_->localAddress().toString(), socket_->peerAddress().toString());
}


void TCPConnection::run()
{
  qDebug() << "Starting connection run loop";

  // TODO: I'm pretty sure that the reconnecting after connection closes does not work.

  started_ = true;

  if(socket_ == 0)
  {
    socket_ = new QTcpSocket();
    QObject::connect(socket_, SIGNAL(bytesWritten(qint64)),
                     this, SLOT(printBytesWritten(qint64)));

    QObject::connect(socket_, SIGNAL(readyRead()),
                     this, SLOT(receivedMessage()));
  }

  if(eventDispatcher() == 0)
  {
    qWarning() << "WARNING: Sorry no event dispatcher for this connection.";
    return;
  }

  while(running_)
  {
    // TODO Stop trying if connection can't be established.
    if(!connected_ && shouldConnect_)
    {
      connectLoop();
    }

    if(socket_->bytesToWrite() > 0)
    {
      qDebug() << "Bytes to write:" << socket_->bytesToWrite() << "in state:" << socket_->state();
    }

    if(connected_)
    {
      if(socket_->isValid() && socket_->canReadLine() && socket_->bytesAvailable() < TOOMANYPACKETS)
      {
        qDebug() << "Can read line with bytes available:" << socket_->bytesAvailable();

        // TODO: maybe not create the data stream everytime.
        // TODO: Check that the bytes available makes sense and that we are no already processing SIP message.
        QDataStream in(socket_);
        in.setVersion(QDataStream::Qt_5_0);
        QString message;
        in >> message;

        emit messageAvailable(message);
      }
      else if(socket_->bytesAvailable() > TOOMANYPACKETS)
      {
        qWarning() << "Flushing the socket because of too much data!";
        socket_->flush();
      }

      sendMutex_.lock();
      while(buffer_.size() > 0 && socket_->state() == QAbstractSocket::ConnectedState)
      {
        bufferToSocket();
      }
      sendMutex_.unlock();
    }

    //qDebug() << "Connection thread waiting";
    eventDispatcher()->processEvents(QEventLoop::WaitForMoreEvents);
    //qDebug() << "Connection thread woken";
  }

  while(buffer_.size() > 0 && socket_->state() == QAbstractSocket::ConnectedState)
  {
    bufferToSocket();
  }

  eventDispatcher()->processEvents(QEventLoop::AllEvents);

  qDebug() << "Disconnecting connection";
  disconnect();

  if(socket_ != 0)
    delete socket_;
}

void TCPConnection::bufferToSocket()
{
  qDebug() << "Sending packet with buffersize:" << buffer_.size();

  if(buffer_.size() > TOOMANYPACKETS)
  {
    qWarning() << "We are sending too much stuff to the other end:" << buffer_.size();
  }

  QString message = buffer_.front();
  buffer_.pop();

  QByteArray block;
  QDataStream out(&block, QIODevice::WriteOnly);
  out.setVersion(QDataStream::Qt_4_0);
  out << message;

  qint64 sentBytes = socket_->write(block);

  qDebug() << "Sent Bytes:" << sentBytes;
}

void TCPConnection::disconnect()
{
  running_ = false;
  shouldConnect_ = false;
  socket_->disconnectFromHost();
  if(socket_->state() == QAbstractSocket::UnconnectedState ||
     socket_->waitForDisconnected(1000))
  {
    qDebug() << "Disconnected";
  }
  else
  {
    emit error(socket_->error(), socket_->errorString());
    return;
  }
  connected_ = false;
}

void TCPConnection::printError(int socketError, const QString &message)
{
  qWarning() << "ERROR. Socket error" << socketError
             << "Error:" << message;
}

void TCPConnection::printBytesWritten(qint64 bytes)
{
  qDebug() << bytes << "bytes written to socket for connection.";
}
