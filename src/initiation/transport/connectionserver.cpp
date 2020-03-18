#include "connectionserver.h"

#include "tcpconnection.h"

#include "common.h"

ConnectionServer::ConnectionServer()
{}

void ConnectionServer::incomingConnection(qintptr socketDescriptor)
{
  printNormal(this, "Incoming TCP connection");
  // create connection
  TCPConnection* con = new TCPConnection();

  con->setExistingConnection(socketDescriptor);

  emit newConnection(con);
}
