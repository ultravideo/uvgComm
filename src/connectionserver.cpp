#include "connectionserver.h"

#include "tcpconnection.h"

ConnectionServer::ConnectionServer()
{}

void ConnectionServer::incomingConnection(qintptr socketDescriptor)
{
  qDebug() << "TCP," << metaObject()->className() << ": Incoming TCP connection";
  // create connection
  TCPConnection* con = new TCPConnection();

  con->setExistingConnection(socketDescriptor);

  emit newConnection(con);
}
