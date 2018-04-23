#pragma once

#include "siptypes.h"

#include <QString>
#include <QList>

#include <memory>

/* This class handles routing information for one SIP dialog.
 * Use this class to get necessary info for via, to, from, contact etc fields in SIP message
 */

class SIPRouting
{
public:
  SIPRouting();

  // RFC3261_TODO: Support Record-route header-field
  // TODO: Use contact-field value with requests.
  // RFC3261_TODO: note, there is a separate RFC for re-INVITE refreshes.

  void setLocalNames(QString username, QString name);
  void setRemoteUsername(QString username);
  void setLocalAddress(QString localAddress);
  void setLocalHost(QString host);
  void setRemoteHost(QString host);

  // returns whether the incoming info is valid.
  // Remember to process request in order of arrival
  // Does not check if we are allowed to receive requests from this user.
  // TODO: Only INVITE requests modify route and contact address. Re-INVITEs only modify contact
  // ( because backward compability )
  bool incomingSIPRequest(std::shared_ptr<SIPRoutingInfo> routing);
  bool incomingSIPResponse(std::shared_ptr<SIPRoutingInfo> routing);

  bool requestForUs(std::shared_ptr<SIPRoutingInfo> routing);
  bool replyToOurRequest(std::shared_ptr<SIPRoutingInfo> routing);

  // returns values to be used in SIP request. Sets directaddress if we have it.
  std::shared_ptr<SIPRoutingInfo> requestRouting(QString& directAddress);

  // copies the fields from previous request
  // sets direct contact as per RFC.
  std::shared_ptr<SIPRoutingInfo> responseRouting();

private:
  QString localName_;
  QString localUsername_;      //our username on sip server
  QString localHost_;          // name of our sip server
  QString localDirectAddress_; // to contact and via fields

  QString remoteUsername_;
  QString remoteHost_;          // name of their sip server or their ip address
  SIP_URI remoteDirectAddress_; // from contact field. Send requests here if not empty.

  std::shared_ptr<SIPRoutingInfo> previousReceivedRequest_;
  std::shared_ptr<SIPRoutingInfo> previousSentRequest_;
};
