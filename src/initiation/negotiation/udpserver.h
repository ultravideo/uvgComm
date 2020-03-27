#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QHostAddress>
#include <QNetworkDatagram>

#include <stdint.h>

class QUdpSocket;

class UDPServer : public QObject
{
  Q_OBJECT
public:
  UDPServer();

  bool bindSocket(const QHostAddress& address, quint16 port);

  void unbind();

  // sends the data using Qt UDP classes.
  bool sendData(QByteArray& data, const QHostAddress &local,
                const QHostAddress &remote, quint16 remotePort);

signals:
  // send message data forward.
  void datagramAvailable(QNetworkDatagram message);

private slots:

  // read the data when it becomes available
  void readDatagram();

private:
  QUdpSocket* socket_;
  uint16_t sendPort_;
};
