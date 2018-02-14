#pragma once
#include <QTcpServer>

class TCPConnection;

class ConnectionServer : public QTcpServer
{
  Q_OBJECT

public:
  ConnectionServer();

signals:

void newConnection(TCPConnection* con);

protected:

  void incomingConnection(qintptr socketDescriptor);
};
