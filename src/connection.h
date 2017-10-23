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
  Connection(uint32_t id, bool sip);

  void stopConnection()
  {
    running_ = false;
    eventDispatcher()->interrupt();
  }

  // establishes a new TCP connection
  void establishConnection(QString &destination, uint16_t port);

  // when a server receives a TCP connection,
  // use this to give the socket to Connection
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
    Q_ASSERT(ID_);
    if(ID_ == 0)
    {
      qCritical() << "ERROR: Connection ID has not been set and it is asked!!";
    }
    return ID_;
  }

  static quint32 generateID()
  {
    static quint32 currentID_ = 0;
    return ++currentID_;
  }

  bool connected()
  {
    if(socket_ != 0){
      return socket_->state() == QAbstractSocket::ConnectedState;
    }
    return false;
  }

  QHostAddress getLocalAddress()
  {
    Q_ASSERT(connected());
    return socket_->localAddress();
  }

  QHostAddress getPeerAddress()
  {
    Q_ASSERT(connected());
    return socket_->peerAddress();
  }

signals:
  void error(int socketError, const QString &message);
  void messageAvailable(QString header, QString content, quint32 id);
  void messageAvailable(QString message, quint32 id);

  // connection has been established
  void connected(quint32 connectionID);

private slots:
  void receivedMessage();
  void printBytesWritten(qint64 bytes);


protected:

  void run();

private:

  void init();

  void printError(int socketError, const QString &message);

  void connectLoop();
  void receiveLoop();
  void sendLoop();

  void bufferToSocket();

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

  bool running_;
  bool started_;

  quint32 ID_; // id for this connection

  QString leftOvers_;

  bool sipParsing_;
};
