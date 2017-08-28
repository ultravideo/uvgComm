#include "connectionserver.h"

#include "connection.h"

ConnectionServer::ConnectionServer()
{}

void ConnectionServer::incomingConnection(qintptr socketDescriptor)
{
  qDebug() << "Incoming TCP connection";
  // create connection
  Connection* con = new Connection(0);

  con->setExistingConnection(socketDescriptor);

  emit newConnection(con);
}
