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

  void sendData(QString data, bool untilReply);

signals:

  void messageAvailable(QString message);

private slots:

  void readData();

private:

  QUdpSocket* socket_;

  QHostAddress peerAddress_;
  quint16 port_;

  QTimer resendTimer_;
  bool waitingReply_;

  int tryNumber_;
};
