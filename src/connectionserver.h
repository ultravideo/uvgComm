#pragma once
#include <QTcpServer>

class Connection;

class ConnectionServer : public QTcpServer
{
  Q_OBJECT

public:
  ConnectionServer();

signals:

void newConnection(Connection* con);

protected:

  void incomingConnection(qintptr socketDescriptor);
};
