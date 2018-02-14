#include "connectionserver.h"

#include "tcpconnection.h"

ConnectionServer::ConnectionServer()
{}

void ConnectionServer::incomingConnection(qintptr socketDescriptor)
{
  qDebug() << "Incoming TCP connection";
  // create connection
  TCPConnection* con = new TCPConnection(0, true);

  con->setExistingConnection(socketDescriptor);

  emit newConnection(con);
}
