#pragma once

#include "siptypes.h"

#include <QString>
#include <QList>

#include <memory>

// This class handles routing information for one SIP dialog.
// Use this class to get necessary info for via, to, from, contact etc fields in SIP message

class SIPRouting
{
public:
  SIPRouting();

  void setLocalUsername(QString username);
  void setRemoteUsername(QString username);
  void setLocalAddress(QString localAddress);
  void setLocalHost(QString host);
  void setRemoteHost(QString host);

  // returns whether the incoming info is valid.
  // Remember to process request in order of arrival
  // Does not check if we are allowed to receive requests from this user.
  bool incomingSIPRequest(std::shared_ptr<SIPRoutingInfo> routing);
  bool incomingSIPResponse(std::shared_ptr<SIPRoutingInfo> routing);

  // returns values to be used in SIP request. Sets directaddress if we have it.
  std::shared_ptr<SIPRoutingInfo> requestRouting(QString& directAddress);

  // copies the fields from previous request
  // sets direct contact as per RFC.
  std::shared_ptr<SIPRoutingInfo> responseRouting();

private:

  QString localUsername_;      //our username on sip server
  QString localHost_;          // name of our sip server
  QString localDirectAddress_; // to contact and via fields

  QString remoteUsername_;
  QString remoteHost_;          // name of their sip server or their ip address
  QString remoteDirectAddress_; // from contact field. Send requests here if not empty.

  std::shared_ptr<SIPRoutingInfo> previousReceivedRequest_;
  std::shared_ptr<SIPRoutingInfo> previousSentRequest_;
};
