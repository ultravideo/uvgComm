#pragma once

#include <QByteArray>
#include <QtNetwork>

#include <queue>

#include <functional>

// handles one connection

class Connection : public QObject
{
  Q_OBJECT
public:
  Connection();

  // establishes a tcp connection
  void establishConnection(QString &destination, uint16_t port);

  void setExistingConnection(qintptr socketDescriptor);

  // sends packet via connection
  void sendPacket(QString &data);

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

  void startLoops();

  void printError(int socketError, const QString &message);

  void connectLoop();
  void receiveLoop();
  void sendLoop();

  void disconnect();

  std::function<void(QByteArray& data)> outDataCallback_;

  QTcpSocket socket_;

  QString destination_;
  uint16_t port_;

  std::queue<QString> buffer_;

  QMutex connectMutex_;
  QMutex sendMutex_;
  QWaitCondition sendCond_;

  bool running_;
};
