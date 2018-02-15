#include "tcpconnection.h"

#include <QDataStream>
#include <QtConcurrent/QtConcurrent>

TCPConnection::TCPConnection()
  :
    socket_(0),
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
  qDebug() << "Constructing connection";
}

void TCPConnection::init()
{
  QObject::connect(this, &error, this, &printError);
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
  const int connectionTimeout = 5 * 1000; // 5 seconds

  qDebug() << "Connecting to address:" << destination_ << "Port:" << port_;

  // TODO Stop trying if connection can't be established.
  socket_->connectToHost(destination_, port_);

  if (!socket_->waitForConnected(connectionTimeout)) {
      emit error(socket_->error(), socket_->errorString());
      return;
  }
  connected_ = true;
  qDebug() << "Outgoing TCP connection established";

  emit socketConnected();
}


void TCPConnection::run()
{
  qDebug() << "Starting connection run loop";

  started_ = true;

  if(socket_ == 0)
  {
    socket_ = new QTcpSocket();
    QObject::connect(socket_, SIGNAL(bytesWritten(qint64)),
                     this, SLOT(printBytesWritten(qint64)));

    QObject::connect(socket_, SIGNAL(readyRead()),
                     this, SLOT(receivedMessage()));
  }

  if(socketDescriptor_ != 0)
  {
    qDebug() << "Getting socket";
    if(!socket_->setSocketDescriptor(socketDescriptor_))
    {
      qCritical() << "ERROR: Could not get socket descriptor";
    }
    qDebug() << "Socket connected to our address:" << socket_->localAddress() << ":" << socket_->peerPort()
             << "Peer address:" << socket_->peerAddress() << ":" << socket_->peerPort();
    connected_ = true;
  }

  if(eventDispatcher() == 0)
  {
    qWarning() << "WARNING: Sorry no event dispatcher for this connection.";
    return;
  }

  while(running_)
  {
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
      if(socket_->canReadLine())
      {
        qDebug() << "Can read line with bytes available:" << socket_->bytesAvailable();

        // TODO: maybe not create the data stream everytime.
        // TODO: Check that the bytes available makes sense and that we are no already processing SIP message.
        QDataStream in(socket_);
        in.setVersion(QDataStream::Qt_4_0);
        QString message;
        in >> message;
        emit messageAvailable(message);
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
  if (socket_->state() == QAbstractSocket::UnconnectedState ||
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
