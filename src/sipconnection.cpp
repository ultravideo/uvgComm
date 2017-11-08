#include "sipconnection.h"

SIPConnection::SIPConnection()
{

}


void SIPConnection::initLocal(QString localUsername, QString localHost, QString sessionHost, QString localAddress)
{
  localUsername_ = localUsername;
  localHost_ = localHost;       // name of our sip server

  localDirectAddress_ = localAddress;
}

// return whether the incoming routing info is valid
bool SIPConnection::incomingSIPRequest(std::shared_ptr<SIPRouting> routing)
{
  if(!checkMessageValidity(routing))
  {
    return false;
  }

  return true;
}

std::shared_ptr<SIPRouting> SIPConnection::requestRouting()
{
  std::shared_ptr<SIPRouting> newRouting(new SIPRouting);

  newRouting->senderUsername = localUsername_;
  newRouting->senderHost = localHost_;
  newRouting->senderReplyAddress.append(localDirectAddress_);
  newRouting->senderDirectAddress = localDirectAddress_;

  newRouting->receiverHost = remoteHost_;

  return newRouting;
}

bool SIPConnection::checkMessageValidity(std::shared_ptr<SIPRouting> routing)
{

}
