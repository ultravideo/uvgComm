#pragma once
#include <QTcpServer>

class TCPConnection;

// a server that monitors TCP connections and emits signal when connection
// is received.

class ConnectionServer : public QTcpServer
{
  Q_OBJECT

public:
  ConnectionServer();

signals:

  // signal to transmit the incoming connection
void newConnection(std::shared_ptr<TCPConnection> con);

protected:

  // a QTcpServer function that is called when we have an incoming connection
  void incomingConnection(qintptr socketDescriptor);
};
