#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QHostAddress>
#include <QNetworkDatagram>

#include <stdint.h>

enum SOCKET_TYPE
{
  SOCKET_NORMAL      = 0,
  SOCKET_RAW         = 1,
  SOCKET_MULTIPLEXED = 2,
};

class QUdpSocket;
class Stun;

class UDPServer : public QObject
{
  Q_OBJECT
public:
  UDPServer();

  void bind(const QHostAddress& address, quint16 port);
  bool bindRaw(const QHostAddress& address, quint16 port);
  bool bindMultiplexed(const QHostAddress& address, quint16 port);

  void unbind();

  // sends the data using Qt UDP classes.
  void sendData(QByteArray& data, const QHostAddress &address, quint16 port, bool untilReply);

  void expectReplyFrom(Stun *stun, QString& address, quint16 port);

signals:
  // send message data forward.
  void messageAvailable(QByteArray message);
  void rawMessageAvailable(QNetworkDatagram message);

private slots:
  // read the data when it becomes available
  void readData();

  // read the data when it becomes available
  void readRawData();

  // read datagram and return it to caller who is listening connection from sender
  void readMultiplexData();

private:
  bool bindSocket(const QHostAddress& address, quint16 port, enum SOCKET_TYPE type);

  QUdpSocket* socket_;

  QMap<QString, QMap<quint16, Stun *>> listeners_;

  QTimer resendTimer_;
  bool waitingReply_;

  int tryNumber_;
  uint16_t sendPort_;
};
