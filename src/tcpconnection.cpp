#include "tcpconnection.h"

#include "common.h"

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
{}

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

    QObject::connect(socket_, &QAbstractSocket::disconnected, this, &TCPConnection::disconnected);
  }
}

void TCPConnection::establishConnection(const QString &destination, uint16_t port)
{
  qDebug() << "Connecting: Establishing connection to" << destination << ":" << port;

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
  qDebug() << "Sending: TO BUFFER.";
  if(active_)
  {
    sendMutex_.lock();
    buffer_.push(data);
    sendMutex_.unlock();

    eventDispatcher()->wakeUp();
  }
  else
  {
    printDebug(DEBUG_WARNING, this, "TCP Send",
                     "Not sending message, because sender has been shut down.");
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
    printDebug(DEBUG_ERROR, this, "TCP Connect", "Socket not initialized before connection");
    return false;
  }

  if(socketDescriptor_ != 0)
  {
    qDebug() << "Setting existing socket:" << socketDescriptor_;
    if(!socket_->setSocketDescriptor(socketDescriptor_))
    {
      printDebug(DEBUG_ERROR, this, "Video Process", "Could not set socket descriptor for existing connection.");
      return false;
    }
  }
  else
  {
    qDebug() << "Connecting: CHECK STATUS. Address:" << destination_ << "Port:" << port_;
    for(unsigned int i = 0; i < NUMBER_OF_RETRIES && socket_->state() != QAbstractSocket::ConnectedState; ++i)
    {
      qDebug() << "Connecting: ATTEMPT CONNECTION. State:" << "Attempt:" << i + 1 << "State:" << socket_->state();
      socket_->connectToHost(destination_, port_);
      socket_->waitForConnected(CONNECTION_TIMEOUT);
    }
  }

  if(socket_->state() != QAbstractSocket::ConnectedState)
  {
    emit error(socket_->error(), socket_->errorString());
    return false;
  }

  qDebug().nospace() << "Connecting: SUCCESS. Local address: "
           << socket_->localAddress().toString() << ":" << socket_->peerPort()
           << " Remote address: " << socket_->peerAddress().toString()
           << ":" << socket_->peerPort();

  emit socketConnected(socket_->localAddress().toString(), socket_->peerAddress().toString());
  return true;
}


void TCPConnection::run()
{
  init();


  if(eventDispatcher() == nullptr)
  {
    printDebug(DEBUG_WARNING, this, "TCP Connect", "No event dispatcher for this connection.");
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
      qDebug() << "Sending: DETECT NEED TO SEND. Bytes to write:" << socket_->bytesToWrite() << "in state:" << socket_->state();
    }

    if(socket_->state() == QAbstractSocket::ConnectedState)
    {
      if(socket_->isValid() && socket_->canReadLine() && socket_->bytesAvailable() < TOO_LARGE_AMOUNT_OF_DATA)
      {
        qDebug() << "Receiving," << metaObject()->className() <<
                    ": Can read line with bytes available:" << socket_->bytesAvailable();

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
        printDebug(DEBUG_WARNING, this, "TCP Receive", "Flushing the socket because of too much data!");
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
  qDebug() << "Sending: START. Buffer size:" << buffer_.size();

  if(buffer_.size() > TOO_LARGE_AMOUNT_OF_DATA)
  {
    printDebug(DEBUG_WARNING, this, "TCP Sending", "We are sending too much stuff to the other end",
                    {"Buffer size"}, {QString::number(buffer_.size())});
  }

  QString message = buffer_.front();
  buffer_.pop();

  QByteArray block;
  QDataStream out(&block, QIODevice::WriteOnly);
  out.setVersion(QDataStream::Qt_4_0);
  out << message;

  qint64 sentBytes = socket_->write(block);

  qDebug() << "Sending: SENT TO SOCKET. Sent bytes:" << sentBytes << "to" << remoteAddress();
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

void TCPConnection::disconnected()
{
  qDebug() << "End connection," << metaObject()->className() << ": Socket disconnected";
}

void TCPConnection::printError(int socketError, const QString &message)
{
  qWarning() << "ERROR. Socket error" << socketError
             << "Error:" << message;
  printDebug(DEBUG_ERROR, this, "TCP", "Socket Error",
             {"Code", "Message"}, {QString::number(socketError), message});
}

void TCPConnection::printBytesWritten(qint64 bytes)
{
  qDebug() << "Sending: WRITTEN." << "Bytes:" << bytes;
}
