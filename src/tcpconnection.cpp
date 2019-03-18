#include "tcpconnection.h"

#include <QDataStream>
#include <QtConcurrent/QtConcurrent>

#include <stdint.h>

const uint32_t TOO_LARGE_AMOUNT_OF_DATA = 100000;

const uint8_t NUMBER_OF_RETRIES = 5;

const uint16_t CONNECTION_TIMEOUT = 2000;


TCPConnection::TCPConnection()
  :
    socket_(nullptr),
    shouldConnect_(false),
    destination_(),
    port_(0),
    socketDescriptor_(0),
    buffer_(),
    sendMutex_(),
    active_(false)
{
  qDebug() << "Constructing TCP connection";
}

void TCPConnection::init()
{
  QObject::connect(this, &TCPConnection::error, this, &TCPConnection::printError);
  active_ = true;

  if(socket_ == nullptr)
  {
    socket_ = new QTcpSocket();
    QObject::connect(socket_, SIGNAL(bytesWritten(qint64)),
                     this, SLOT(printBytesWritten(qint64)));

    QObject::connect(socket_, SIGNAL(readyRead()),
                     this, SLOT(receivedMessage()));
  }
}

void TCPConnection::establishConnection(const QString &destination, uint16_t port)
{
  qDebug() << "Establishing connection to" << destination << ":" << port;

  destination_ = destination;
  port_ = port;
  shouldConnect_ = true;
  start();
}

void TCPConnection::setExistingConnection(qintptr socketDescriptor)
{
  qDebug() << "Setting existing/incoming connection. SocketDesc:" << socketDescriptor;
  socketDescriptor_ = socketDescriptor;
  shouldConnect_ = true;
  start();
}

void TCPConnection::sendPacket(const QString &data)
{
  qDebug() << "adding packet for sending";
  if(active_)
  {
    sendMutex_.lock();
    buffer_.push(data);
    sendMutex_.unlock();

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
  if (active_ )
  {
    eventDispatcher()->wakeUp();
  }
  else {
    qDebug() << "";
  }
}

bool TCPConnection::connectLoop()
{
  Q_ASSERT(socket_);

  if (!socket_)
  {
    qWarning() << "ERROR: Socket not initialized before connection";
    return false;
  }

  if(socketDescriptor_ != 0)
  {
    qDebug() << "Setting existing socket:" << socketDescriptor_;
    if(!socket_->setSocketDescriptor(socketDescriptor_))
    {
      qCritical() << "ERROR: Could not set socket descriptor for existing connection";
      return false;
    }
  }
  else
  {
    qDebug() << "Connecting to address:" << destination_ << "Port:" << port_;

    for(unsigned int i = 0; i < NUMBER_OF_RETRIES && socket_->state() != QAbstractSocket::ConnectedState; ++i)
    {
      qDebug() << "Current state:" << socket_->state() << "Trying to connect socket. Attempt:" << i + 1;
      socket_->connectToHost(destination_, port_);
      socket_->waitForConnected(CONNECTION_TIMEOUT);
    }
  }

  if(socket_->state() != QAbstractSocket::ConnectedState)
  {
    emit error(socket_->error(), socket_->errorString());
    return false;
  }

  qDebug().nospace() << "Connecting succeeded. Local address: "
           << socket_->localAddress().toString() << ":" << socket_->peerPort()
           << " Remote address: " << socket_->peerAddress().toString()
           << ":" << socket_->peerPort();

  emit socketConnected(socket_->localAddress().toString(), socket_->peerAddress().toString());
  return true;
}


void TCPConnection::run()
{
  qDebug() << "Started connection";
  init();


  if(eventDispatcher() == nullptr)
  {
    qWarning() << "WARNING: Sorry no event dispatcher for this connection.";
    return;
  }
  while(active_)
  {
    // TODO Stop trying if connection can't be established.
    if(socket_->state() != QAbstractSocket::ConnectedState && shouldConnect_)
    {
      if (!connectLoop())
      {
        qDebug() << "Failed to connect the TCP connection";
      }
    }

    if(socket_->bytesToWrite() > 0)
    {
      qDebug() << "Bytes to write:" << socket_->bytesToWrite() << "in state:" << socket_->state();
    }

    if(socket_->state() == QAbstractSocket::ConnectedState)
    {
      if(socket_->isValid() && socket_->canReadLine() && socket_->bytesAvailable() < TOO_LARGE_AMOUNT_OF_DATA)
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
      else if(socket_->bytesAvailable() > TOO_LARGE_AMOUNT_OF_DATA)
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

    eventDispatcher()->processEvents(QEventLoop::WaitForMoreEvents);
  }

  while(buffer_.size() > 0 && socket_->state() == QAbstractSocket::ConnectedState)
  {
    bufferToSocket();
  }

  eventDispatcher()->processEvents(QEventLoop::ExcludeUserInputEvents);

  qDebug() << "Disconnecting connection";
  disconnect();

  if(socket_ != nullptr)
  {
    delete socket_;
    socket_ = nullptr;
  }
}

void TCPConnection::bufferToSocket()
{
  qDebug() << "Sending packet with buffersize:" << buffer_.size();

  if(buffer_.size() > TOO_LARGE_AMOUNT_OF_DATA)
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
  active_ = false;
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
