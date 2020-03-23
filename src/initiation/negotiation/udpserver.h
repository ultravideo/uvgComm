#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QHostAddress>
#include <QNetworkDatagram>

#include <stdint.h>

class QUdpSocket;
class Stun;

class UDPServer : public QObject
{
  Q_OBJECT
public:
  UDPServer();

  bool bindSocket(const QHostAddress& address, quint16 port, bool multiplexed);

  void unbind();

  // sends the data using Qt UDP classes.
  bool sendData(QByteArray& data, const QHostAddress &local,
                const QHostAddress &remote, quint16 remotePort);

  void expectReplyFrom(Stun *stun, QString& address, quint16 port);

signals:
  // send message data forward.
  void datagramAvailable(QNetworkDatagram message);

private slots:

  // read the data when it becomes available
  void readDatagram();

  // read datagram and return it to caller who is listening connection from sender
  void readMultiplexData();

private:
  QUdpSocket* socket_;

  QMap<QString, QMap<quint16, Stun *>> listeners_;

  QTimer resendTimer_;
  bool waitingReply_;

  int tryNumber_;
  uint16_t sendPort_;
};
