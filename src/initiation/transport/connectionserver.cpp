#include "connectionserver.h"

#include "tcpconnection.h"

#include "common.h"

ConnectionServer::ConnectionServer()
{}

void ConnectionServer::incomingConnection(qintptr socketDescriptor)
{
  printNormal(this, "Incoming TCP connection");
  // create connection
  std::shared_ptr<TCPConnection> con = std::shared_ptr<TCPConnection> (new TCPConnection());

  con->setExistingConnection(socketDescriptor);

  emit newConnection(con);
}
