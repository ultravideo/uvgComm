#include "sipconnection.h"

#include <QDebug>

SIPConnection::SIPConnection():
  localUsername_(""),
  localHost_(""),
  localDirectAddress_(""),
  remoteUsername_(""),
  remoteHost_(""),
  sessionHost_(""),
  previousReceivedRequest_(NULL),
  previousSentRequest_(NULL)
{}

void SIPConnection::initLocal(QString localUsername, QString localHost, QString localAddress, QString sessionHost)
{
  localUsername_ = localUsername;
  localHost_ = localHost;       // name of our sip server
  localDirectAddress_ = localAddress;
}

void SIPConnection::initRemote(QString remoteUsername, QString remoteHost)
{
  remoteUsername_ = remoteUsername;
  remoteHost_ = remoteHost;
}

bool SIPConnection::incomingSIPRequest(std::shared_ptr<SIPRouting> routing)
{
  if(remoteUsername_ == "" && remoteHost_ == "")
  {
    remoteUsername_ = routing->senderUsername;
    remoteHost_ = routing->senderHost;
  }

  if(routing->senderUsername != remoteUsername_ ||
     routing->senderHost != remoteHost_ ||
     routing->receiverUsername != localUsername_ ||
     routing->receiverHost != localHost_)
  {
    qDebug() << "Incoming SIP Request sender:" << routing->senderUsername << "@" << routing->senderHost
             << "->" << routing->receiverUsername << "@" << routing->receiverHost << "does not match local info. Them:"
             << remoteUsername_ << "@" << remoteHost_ << "-> Us" << localUsername_ << "@" << localHost_;
    return false;
  }
  previousReceivedRequest_ = routing;
  return true;
}

bool SIPConnection::incomingSIPResponse(std::shared_ptr<SIPRouting> routing)
{
  if(previousSentRequest_ == NULL)
  {
    qDebug() << "OTHER ERROR: Got a response without sending any requests!!!";
    return false;
  }

  // check if this response matches the request sent.
  if(routing->senderUsername   != previousSentRequest_->senderUsername ||
     routing->senderHost       != previousSentRequest_->senderHost ||
     routing->receiverUsername != previousSentRequest_->receiverUsername ||
     routing->receiverHost     != previousSentRequest_->receiverHost ||
     routing->sessionHost != previousSentRequest_->sessionHost)
  {
    qDebug() << "OTHER ERROR: The incoming SIP response "
                "does not have the same fields as the sent request";
    return false;
  }

  if(routing->contactAddress == previousSentRequest_->contactAddress)
  {
    qDebug() << "OTHER ERROR: They have not set their contact field correctly";
    return false;
  }

  qDebug() << "Request via:" << previousSentRequest_->senderReplyAddress.at(0)
           << "Response # vias:" << routing->senderReplyAddress.size();

  remoteDirectAddress_ = routing->contactAddress;
}

std::shared_ptr<SIPRouting> SIPConnection::requestRouting(QString &directAddress)
{
  if(localUsername_.isEmpty() ||
     localHost_.isEmpty() ||
     localDirectAddress_.isEmpty() ||
     remoteUsername_.isEmpty() ||
     remoteHost_.isEmpty() ||
     sessionHost_.isEmpty())
  {
    qDebug() << "WARNING: Trying to get request routing without initialization!!";
    return NULL;
  }

  std::shared_ptr<SIPRouting> newRouting(new SIPRouting);

  newRouting->senderUsername = localUsername_;
  newRouting->senderHost = localHost_;
  newRouting->senderReplyAddress.append(localDirectAddress_);
  newRouting->contactAddress = localDirectAddress_;

  newRouting->receiverHost = remoteUsername_;
  newRouting->receiverHost = remoteHost_;

  newRouting->sessionHost = sessionHost_;

  if(!remoteDirectAddress_.isEmpty())
  {
    directAddress = remoteDirectAddress_;
  }

  return newRouting;
}

// copies the fields from previous request
std::shared_ptr<SIPRouting> SIPConnection::responseRouting()
{
  Q_ASSERT(previousReceivedRequest_);
  if(!previousReceivedRequest_)
  {
    qDebug() << "WARNING: Trying to send a response when "
                "there is no previous request to copy for response routing";
    return NULL;
  }

  std::shared_ptr<SIPRouting> newRouting(new SIPRouting);
  //everything expect contact field are copied from received request
  *newRouting = *previousReceivedRequest_;
  newRouting->contactAddress = localDirectAddress_;

  return newRouting;
}
