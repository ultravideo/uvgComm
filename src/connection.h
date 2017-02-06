#pragma once

#include <QByteArray>
#include <QtNetwork>

#include <functional>

// handles one connection

class Connection : public QThread
{
  Q_OBJECT
public:
  Connection();

  // establishes a tcp connection
  void establishConnection(QString &destination, uint16_t port);

  void setConnection(QString &destination, uint16_t port);

  // sends packet via connection
  void sendPacket(QByteArray& data);

  // callback
  template <typename Class>
  void addDataOutCallback (Class* o, void (Class::*method) (QByteArray& data))
  {
    outDataCallback_ = ([o,method] (QByteArray& data)
    {
      return (o->*method)(data);
    });
  }

signals:
  void error(int socketError, const QString &message);
  void messageReceived(QString message);

private:

  void run();

  void printError(int socketError, const QString &message);

  std::function<void(QByteArray& data)> outDataCallback_;

  QTcpSocket socket_;
  QWaitCondition wait_;
  QMutex mutex_;

  QString destination_;
  uint16_t port_;

  // whether we should initiate a new connection
  bool connect_;

  bool running_;
};
