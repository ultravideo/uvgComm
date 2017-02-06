#include "connection.h"

#include <QDataStream>

Connection::Connection()
{}

void Connection::establishConnection(QString &destination, uint16_t port)
{
  destination_ = destination;
  port_ = port;
  start();
}

void Connection::sendPacket(QByteArray& data)
{
  mutex_.lock();

  if(!isRunning())
    start();
  else
    wait_.wakeOne();

  mutex_.unlock();
}

void Connection::run()
{
  QString serverName = destination_;
  quint16 serverPort = port_;

  while(running_)
  {
    // start connection
    connect(this, &error, this, &printError);


    const int connectionTimeout = 5 * 1000;
    const int packetTimeout = 300 * 1000;

    socket_.connectToHost(destination_, port_);

    if (!socket_.waitForConnected(connectionTimeout)) {
        emit error(socket_.error(), socket_.errorString());
        return;
    }

    QDataStream in(&socket_);
    in.setVersion(QDataStream::Qt_4_0);
    QString message;

    if (!socket_.waitForReadyRead(packetTimeout)) {
      emit error(socket_.error(), socket_.errorString());
      return;
    }

    in >> message;
    qDebug().noquote() << "Received the following message:" << message;

    mutex_.lock();
    emit messageReceived(message);

    wait_.wait(&mutex_);
    serverName = destination_;
    serverPort = port_;
    mutex_.unlock();
  }
}

void Connection::printError(int socketError, const QString &message)
{
  qWarning() << "ERROR: " << message;
}
