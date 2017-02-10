#include "connection.h"

#include <QDataStream>

#include <QtConcurrent/QtConcurrent>

Connection::Connection()
{}

void Connection::establishConnection(QString &destination, uint16_t port)
{

  QObject::connect(this, &error, this, &printError);

  destination_ = destination;
  port_ = port;

  connectMutex_.lock();
  QtConcurrent::run(this, &Connection::connectLoop);
  connectMutex_.unlock();
  startLoops();
}

void Connection::setExistingConnection(qintptr socketDescriptor)
{
  QObject::connect(this, &error, this, &printError);

  socket_.setSocketDescriptor(socketDescriptor);
  startLoops();
}

void Connection::sendPacket(QString& data)
{
  if(running_)
  {
    buffer_.push(data);
    sendCond_.wakeOne();
  }
  else
  {
    qWarning() << "Warning: Not sending message, because sender has been shut down.";
  }
}

void Connection::startLoops()
{
  QtConcurrent::run(this, &Connection::sendLoop);
  QtConcurrent::run(this, &Connection::receiveLoop);
}

void Connection::connectLoop()
{
  const int connectionTimeout = 5 * 1000; // 5 seconds

  socket_.connectToHost(destination_, port_);

  if (!socket_.waitForConnected(connectionTimeout)) {
      emit error(socket_.error(), socket_.errorString());
      return;
  }

  qDebug() << "TCP connection established";
}

void Connection::receiveLoop()
{
  const int packetTimeout = 300 * 1000; // 5 minutes

  connectMutex_.lock();
  connectMutex_.unlock();

  while(running_)
  {
    QDataStream in(&socket_);
    in.setVersion(QDataStream::Qt_4_0);
    QString message;

    if (!socket_.waitForReadyRead(packetTimeout)) {
      emit error(socket_.error(), socket_.errorString());
      return;
    }

    in >> message;
    qDebug().noquote() << "Received the following message:" << message;

    emit messageReceived(message, ID_);
  }
}

void Connection::sendLoop()
{
  connectMutex_.lock();
  connectMutex_.unlock();

  while(running_)
  {
    sendMutex_.lock();
    sendCond_.wait(&sendMutex_);
    sendMutex_.unlock();

    QString message = "";

    if(!buffer_.empty())
    {
      message = buffer_.front();
      buffer_.pop();
    }
    else
    {
      qWarning() << "WARNING: Trying to process send buffer when it was empty";
    }

    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_4_0);
    out << message;

    socket_.write(block);
  }
}

void Connection::disconnect()
{
  running_ = false;
  socket_.disconnectFromHost();
  socket_.waitForDisconnected();
}

void Connection::printError(int socketError, const QString &message)
{
  qWarning() << "ERROR: " << message;
}
