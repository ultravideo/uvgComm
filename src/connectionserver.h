#pragma once


#include <QTcpServer>

class ConnectionServer : public QTcpServer
{
public:
  ConnectionServer();

protected:

  //
  void incomingConnection(qintptr socketDescriptor);
};
