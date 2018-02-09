#include "siprouting.h"

#include <QDebug>

const uint16_t MAXFORWARDS = 70; // the recommmended value is 70

SIPRouting::SIPRouting():
  localUsername_(""),
  localHost_(""),
  localDirectAddress_(""),
  remoteUsername_(""),
  remoteHost_(""),
  previousReceivedRequest_(NULL),
  previousSentRequest_(NULL)
{}

void SIPRouting::setLocalNames(QString username, QString name)
{
  localUsername_ = username;
  localName_ = name;
}

void SIPRouting::setRemoteUsername(QString username)
{
  remoteUsername_ = username;
}

void SIPRouting::setLocalAddress(QString localAddress)
{
  localDirectAddress_ = localAddress;
}

void SIPRouting::setLocalHost(QString host)
{
  localHost_ = host;
}

void SIPRouting::setRemoteHost(QString host)
{
  remoteHost_ = host;
}

bool SIPRouting::incomingSIPRequest(std::shared_ptr<SIPRoutingInfo> routing)
{
  if(remoteUsername_ == "" && remoteHost_ == "")
  {
    remoteUsername_ = routing->from.username;
    remoteHost_ = routing->from.host;
  }

  if(routing->from.username != remoteUsername_ ||
     routing->from.host != remoteHost_ ||
     routing->to.username != localUsername_ ||
     routing->to.host != localHost_)
  {
    qDebug() << "Incoming SIP Request sender:" << routing->from.username << "@" << routing->from.host
             << "->" << routing->to.username << "@" << routing->to.host << "does not match local info. Them:"
             << remoteUsername_ << "@" << remoteHost_ << "-> Us" << localUsername_ << "@" << localHost_;
    return false;
  }
  previousReceivedRequest_ = routing;
  return true;
}

bool SIPRouting::incomingSIPResponse(std::shared_ptr<SIPRoutingInfo> routing)
{
  if(previousSentRequest_ == NULL)
  {
    qDebug() << "OTHER ERROR: Got a response without sending any requests!!!";
    return false;
  }

  // check if this response matches the request sent.
  if(routing->from.username   != previousSentRequest_->from.username ||
     routing->from.host       != previousSentRequest_->from.host ||
     routing->to.username != previousSentRequest_->to.username ||
     routing->to.host     != previousSentRequest_->to.host ||
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

  qDebug() << "Request via:" << previousSentRequest_->senderReplyAddress.at(0).address
           << "Response # vias:" << routing->senderReplyAddress.size();

  remoteDirectAddress_ = routing->contactAddress;
  return true;
}

std::shared_ptr<SIPRoutingInfo> SIPRouting::requestRouting(QString &directAddress)
{
  if(localUsername_.isEmpty() ||
     localHost_.isEmpty() ||
     localDirectAddress_.isEmpty() ||
     remoteUsername_.isEmpty() ||
     remoteHost_.isEmpty() ||
     localUsername_ == "" ||
     localHost_ == "" ||
     localDirectAddress_ == "" ||
     remoteUsername_ == ""  ||
     remoteHost_ == "")
  {
    qDebug() << "WARNING: Trying to get request routing without proper initialization!!";
    return NULL;
  }

  std::shared_ptr<SIPRoutingInfo> newRouting(new SIPRoutingInfo);

  newRouting->from.username = localUsername_;
  newRouting->from.realname = localName_;
  newRouting->from.host = localHost_;
  newRouting->senderReplyAddress.append(ConnectInstructions {TCP, "2.0", localDirectAddress_});
  newRouting->contactAddress = localDirectAddress_;

  newRouting->to.realname = remoteUsername_;
  newRouting->to.username = remoteUsername_;
  newRouting->to.host = remoteHost_;

  newRouting->sessionHost = localHost_;

  newRouting->maxForwards = MAXFORWARDS;

  if(!remoteDirectAddress_.isEmpty())
  {
    directAddress = remoteDirectAddress_;
  }

  return newRouting;
}

// copies the fields from previous request
std::shared_ptr<SIPRoutingInfo> SIPRouting::responseRouting()
{
  Q_ASSERT(previousReceivedRequest_);
  if(!previousReceivedRequest_)
  {
    qDebug() << "WARNING: Trying to send a response when "
                "there is no previous request to copy for response routing";
    return NULL;
  }

  std::shared_ptr<SIPRoutingInfo> newRouting(new SIPRoutingInfo);
  //everything expect contact field are copied from received request
  *newRouting = *previousReceivedRequest_;
  newRouting->contactAddress = localDirectAddress_;

  // I may be wrong, but I did not reset maxforwards

  return newRouting;
}
