#pragma once

#include <QByteArray>
#include <QtNetwork>

#include <queue>

#include <functional>

// handles one connection




class Connection : public QThread
{
  Q_OBJECT
public:
  Connection();

  // establishes a tcp connection
  void establishConnection(QString &destination, uint16_t port);

  void setExistingConnection(qintptr socketDescriptor);

  // sends packet via connection
  void sendPacket(const QString &data);

  // callback
  template <typename Class>
  void addDataOutCallback (Class* o, void (Class::*method) (QByteArray& data))
  {
    outDataCallback_ = ([o,method] (QByteArray& data)
    {
      return (o->*method)(data);
    });
  }

  void setID(uint32_t id)
  {
    ID_ = id;
  }

  uint32_t getID()
  {
    return ID_;
  }

  static uint32_t generateID()
  {
    static uint32_t currentID_ = 1;
    return ++currentID_;
  }

signals:
  void error(int socketError, const QString &message);
  void messageReceived(QString message, uint32_t id);

private slots:
  //void receivedMessage();

  void printBytesWritten(qint64 bytes);

protected:

  void run();

private:

  void init();

  void printError(int socketError, const QString &message);

  void connectLoop();
  void receiveLoop();
  void sendLoop();

  void disconnect();

  std::function<void(QByteArray& data)> outDataCallback_;

  QTcpSocket *socket_;

  bool shouldConnect_;
  bool connected_;

  QString destination_;
  uint16_t port_;

  qintptr socketDescriptor_;
  std::queue<QString> buffer_;

  QMutex sendMutex_;
  QMutex checkMutex_;
  QWaitCondition checkCond_;

  bool running_;

  uint32_t ID_; // id for this connection
};