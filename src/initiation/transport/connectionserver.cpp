#include "connectionserver.h"

#include "tcpconnection.h"

#include "common.h"
#include "global.h"
#include "logger.h"


ConnectionServer::ConnectionServer()
{}

void ConnectionServer::incomingConnection(qintptr socketDescriptor)
{
  Logger::getLogger()->printNormal(this, "Incoming TCP connection");
  // create connection
  TCPConnection* con = new TCPConnection();

  con->setExistingConnection(socketDescriptor);

  emit newConnection(con);
}
