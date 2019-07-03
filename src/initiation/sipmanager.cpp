#include "sipmanager.h"

#include <QObject>

SIPManager::SIPManager():
  tcpServer_(),
  sipPort_(5060) // default for SIP, use 5061 for tls encrypted
{

}


// start listening to incoming
void SIPManager::init(SIPTransactionUser* callControl, SIPTransactions& transactions)
{
  QObject::connect(&tcpServer_, &ConnectionServer::newConnection,
                   &transactions, &SIPTransactions::receiveTCPConnection);

  tcpServer_.setProxy(QNetworkProxy::NoProxy);

  // listen to everything
  qDebug() << "Initiating," << metaObject()->className()
           << ": Listening to SIP TCP connections on port:" << sipPort_;
  if (!tcpServer_.listen(QHostAddress::Any, sipPort_))
  {
    printDebug(DEBUG_ERROR, this, DC_STARTUP,
               "Failed to listen to socket. Is it reserved?");

    // TODO announce it to user!
  }

  transactions.init(callControl);
}


void SIPManager::uninit(SIPTransactions& transactions)
{
  transactions.uninit();
}
