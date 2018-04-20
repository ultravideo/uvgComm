#pragma once
#include <QByteArray>
#include <QtNetwork>

#include <queue>
#include <functional>

// handles one connection
// TODO: Implement a keep-alive CRLF sending.


class TCPConnection : public QThread
{
  Q_OBJECT
public:
  TCPConnection();

  void stopConnection()
  {
    running_ = false;
    eventDispatcher()->interrupt();
  }

  // establishes a new TCP connection
  void establishConnection(QString const &destination, uint16_t port);

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

signals:
  void error(int socketError, const QString &message);
  void messageAvailable(QString message);

  // connection has been established
  void socketConnected(QString localAddress, QString remoteAddress);

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

  QString leftOvers_;
};
