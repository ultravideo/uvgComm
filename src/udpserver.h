#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QHostAddress>

class QUdpSocket;

class UDPServer : public QObject
{
  Q_OBJECT
public:
  UDPServer();

  void bind(const QHostAddress &address, quint16 port);

  void sendData(QByteArray& data, const QHostAddress &address, quint16 port, bool untilReply);

signals:

  void messageAvailable(QByteArray message);

private slots:

  void readData();

private:

  QUdpSocket* socket_;

  QTimer resendTimer_;
  bool waitingReply_;

  int tryNumber_;
  uint16_t sendPort_;
};
