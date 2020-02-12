#pragma once
#include <QByteArray>
#include <QtNetwork>

#include <queue>
#include <functional>

#include <stdint.h>

// handles one connection
// TODO: Implement a keep-alive CRLF sending.
// TODO: This class is a bit wonky at the moment,
// needs general improvement in functionality

class TCPConnection : public QThread
{
  Q_OBJECT
public:
  TCPConnection();
  ~TCPConnection();

  void stopConnection()
  {
    active_ = false;
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

  bool isConnected() const
  {
    return socket_ && socket_->state() == QAbstractSocket::ConnectedState;
  }

  // TODO: Returns empty if we are not connected to anything.
  QHostAddress localAddress()
  {
    Q_ASSERT(socket_);
    Q_ASSERT(socket_->state() == QAbstractSocket::ConnectedState);
    Q_ASSERT(socket_->localAddress().toString() != "");
    return socket_->localAddress();
  }

  uint16_t localPort() const
  {
    Q_ASSERT(socket_);
    Q_ASSERT(socket_->state() == QAbstractSocket::ConnectedState);
    return socket_->localPort();
  }

  QHostAddress remoteAddress()
  {
    Q_ASSERT(socket_);
    Q_ASSERT(socket_->state() == QAbstractSocket::ConnectedState);
    Q_ASSERT(socket_->peerAddress().toString() != "");

    if (!socket_)
    {
      return QHostAddress();
    }

    return socket_->peerAddress();
  }

signals:
  void error(int socketError, const QString &message);
  void messageAvailable(QString message);

  // connection has been established
  void socketConnected(QString localAddress, QString remoteAddress);

private slots:
  void receivedMessage();
  void printBytesWritten(qint64 bytes);

  void disconnected();

protected:

  void run();

private:

  // connects signals.
  void init();
  void uninit();

  void printError(int socketError, const QString &message);

  // return if succeeded
  bool connectLoop();
  void receiveLoop();
  void sendLoop();

  void bufferToSocket();

  void disconnect();

  std::function<void(QByteArray& data)> outDataCallback_;

  QTcpSocket *socket_;

  bool shouldConnect_;

  QString destination_;
  uint16_t port_;

  qintptr socketDescriptor_;
  std::queue<QString> buffer_;

  QMutex sendMutex_;

  // Indicates whether the connection is active or disconnected
  bool active_;

  QMutex readWriteMutex_;

  QString leftOvers_;
};
