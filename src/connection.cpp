#include "connection.h"

#include <QDataStream>

#include <QtConcurrent/QtConcurrent>

Connection::Connection()
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
    started_(false),
    ID_(generateID())
{
  qDebug() << "Creating connection with ID:" << ID_;
}

void Connection::init()
{
  QObject::connect(this, &error, this, &printError);
  running_ = true;

  start();
}

void Connection::establishConnection(QString &destination, uint16_t port)
{
  qDebug() << "Establishing connection to" << destination << ":" << port;

  destination_ = destination;
  port_ = port;
  shouldConnect_ = true;
  init();
}

void Connection::setExistingConnection(qintptr socketDescriptor)
{
  qDebug() << "Setting existing/incoming connection. SocketDesc:" << socketDescriptor;
  socketDescriptor_ = socketDescriptor;
  init();
}

void Connection::sendPacket(const QString &data)
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

void Connection::receivedMessage()
{
  qDebug() << "Socket ready to read:" << ID_;

  if(started_)
    eventDispatcher()->wakeUp();
}

void Connection::connectLoop()
{
  const int connectionTimeout = 5 * 1000; // 5 seconds

  socket_->connectToHost(destination_, port_);

  if (!socket_->waitForConnected(connectionTimeout)) {
      emit error(socket_->error(), socket_->errorString());
      return;
  }
  connected_ = true;
  qDebug() << "Outgoing TCP connection established";
}


void Connection::run()
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
    connected_ = true;
  }

  if(eventDispatcher() == 0)
  {
    qWarning() << "WARNING: Sorry no event dispatcher.";
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
        QDataStream in(socket_);
        in.setVersion(QDataStream::Qt_4_0);
        QString message;

        in >> message;

        if(leftOvers_.length() > 0)
        {
          message = leftOvers_ + message;
        }

        int headerEndIndex = message.indexOf("\r\n\r\n", 0, Qt::CaseInsensitive) + 4;
        int contentLengthIndex = message.indexOf("content-length", 0, Qt::CaseInsensitive);

        qDebug() << "header end at:" << headerEndIndex
                 << "and content-length at:" << contentLengthIndex;

        if(contentLengthIndex != -1 && headerEndIndex != -1)
        {
          int contentLengthLineEndIndex = message.indexOf("\r\n", contentLengthIndex, Qt::CaseInsensitive);

          QString value = message.mid(contentLengthIndex + 16, contentLengthLineEndIndex - (contentLengthIndex + 16));

          int valueInt= value.toInt();

          qDebug() << "Content-length:" <<  valueInt;

          if(message.length() < headerEndIndex + valueInt)
          {
            leftOvers_ = message;
          }
          else
          {
            leftOvers_ = message.right(message.length() - (headerEndIndex + valueInt));
            QString header = message.left(headerEndIndex);
            QString content = message.mid(headerEndIndex, valueInt);

            qDebug() << "Whole message received.";
            qDebug() << "Header:" << header;
            qDebug() << "Content:" << content;
            qDebug() << "Left overs:" << leftOvers_;

            emit messageAvailable(header, content, ID_);
          }
        }
        else
        {
          qDebug() << "Message was not received fully";
          leftOvers_ = message;
        }
      }

      sendMutex_.lock();
      while(buffer_.size() > 0 && socket_->state() == QAbstractSocket::ConnectedState)
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
      sendMutex_.unlock();
    }

    qDebug() << "Connection thread waiting:" << ID_;
    eventDispatcher()->processEvents(QEventLoop::WaitForMoreEvents);
    qDebug() << "Connection thread woken:" << ID_;
  }

  disconnect();

  if(socket_ != 0)
    delete socket_;
}

void Connection::disconnect()
{
  running_ = false;
  shouldConnect_ = false;
  socket_->disconnectFromHost();
  socket_->waitForDisconnected();
  connected_ = false;
}

void Connection::printError(int socketError, const QString &message)
{
  qWarning() << "ERROR. Socket error for connection:" << ID_ << "Error:" << message;
}

void Connection::printBytesWritten(qint64 bytes)
{
  qDebug() << bytes << "bytes written to socket for connection ID:" << ID_;
}
