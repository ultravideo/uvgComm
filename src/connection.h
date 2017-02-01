#pragma once

#include <QByteArray>
#include <QtNetwork>

// handles one connection

class Connection : public QThread
{
public:
  Connection();

  void sendPacket(QByteArray& data, QHostAddress &destination, uint16_t port);

  void connect();

private:

  void run();

  QTcpSocket socket_;
  QWaitCondition cond_;
  QMutex mutex_;

  QHostAddress destination_;
  uint16_t port_;
};
