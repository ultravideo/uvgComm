#pragma once

#include <QString>
#include <QList>

#include <memory>

struct SIPRouting
{
  //what about request vs response?

  QString senderUsername;
  QString senderHost;
  QList<QString> senderReplyAddress;   // from via-fields. Send responses here by copying these.
  QString senderDirectAddress;  // from contact field. Send requests here

  QString receiverUsername;
  QString receiverHost;

  QString sessionHost;
};

// This class handles routing information for one SIP dialog.
// Use this class to get necessary info for via, to, from, contact etc fields in SIP message

class SIPConnection
{
public:
  SIPConnection();

  // if localHost is empty, assume this is a peer to peer session
  void initLocal(QString localUsername, QString localHost, QString sessionHost, QString localAddress);

  void initRemote(QString remoteUsername, QString remoteHost);

  bool incomingSIPRequest(std::shared_ptr<SIPRouting> routing);
  bool incomingSIPResponse(std::shared_ptr<SIPRouting> routing);

  std::shared_ptr<SIPRouting> requestRouting();

  // copies the fields from previous request
  std::shared_ptr<SIPRouting> responseRouting();

private:

  bool checkMessageValidity(std::shared_ptr<SIPRouting> routing);

  QString localUsername_;
  QString localHost_;          // name of our sip server
  QString localDirectAddress_; // to contact and via fields

  QString remoteUsername_;
  QString remoteHost_;          // name of their sip server
  QString remoteDirectAddress_; // from contact field

  QList<QString> remoteReplyAddress_;   // from via-field
  QString remoteContactAddress_;

  QString sessionHost_;
};
