#include "sipmanager.h"

#include <QObject>

SIPManager::SIPManager():
  tcpServer_(),
  sipPort_(5060), // default for SIP, use 5061 for tls encrypted
  transactions_()
{

}


// start listening to incoming
void SIPManager::init(SIPTransactionUser* callControl)
{
  QObject::connect(&tcpServer_, &ConnectionServer::newConnection,
                   &transactions_, &SIPTransactions::receiveTCPConnection);

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

  transactions_.init(callControl);
}


void SIPManager::uninit()
{
  transactions_.uninit();
}


void SIPManager::bindToServer()
{
  transactions_.bindToServer();
}


uint32_t SIPManager::startCall(Contact& address)
{
  return transactions_.startCall(address);
}

void SIPManager::acceptCall(uint32_t sessionID)
{
  transactions_.acceptCall(sessionID);
}

void SIPManager::rejectCall(uint32_t sessionID)
{
  transactions_.rejectCall(sessionID);
}

void SIPManager::cancelCall(uint32_t sessionID)
{
  transactions_.cancelCall(sessionID);
}

void SIPManager::endCall(uint32_t sessionID)
{
  transactions_.endCall(sessionID);
}

void SIPManager::endAllCalls()
{
  transactions_.endAllCalls();
}

void SIPManager::getSDPs(uint32_t sessionID,
             std::shared_ptr<SDPMessageInfo>& localSDP,
             std::shared_ptr<SDPMessageInfo>& remoteSDP)
{
  transactions_.getSDPs(sessionID, localSDP, remoteSDP);
}
