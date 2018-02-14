#pragma once
#include "siptypes.h"

#include <QHostInfo>
#include <QHostAddress>
#include <QString>

/* Handles creation of SIP requests and responses on at a time.
 * Will not check the legality of the request/response,
 * only that all required fields are provided.
 * Usage: call startRequest or startResponse */

#include <memory>

typedef int16_t messageID;

class SIPStringComposer
{
public:
  SIPStringComposer();


  // MANDATORY ---------------------------

  messageID startSIPRequest(const RequestType request, const QString& SIPversion = "2.0");
  messageID startSIPResponse(const ResponseType response, const QString& SIPversion = "2.0");

  // include their tag only if it was already provided
  void to(messageID id, QString& name, QString& username, const QString& hostname, const QString& tag = "");

  void from(messageID id, QString& name, QString& username, const QHostInfo& hostname, const QString& tag);
  void fromIP(messageID id, QString& name, QString& username, const QHostAddress& address, const QString& tag);

  // Where to send responses. branch is generated.
  // Also adds the contact (where to send requests) field with same info.
  void via(messageID id, const QHostInfo& hostname, const QString& branch);
  void viaIP(messageID id, QHostAddress address, QString& branch);

  void maxForwards(messageID id, uint16_t forwards);

  // string must include host!
  void setCallID(messageID id, QString& callID, QString& host);

  void sequenceNum(messageID id, uint32_t seq, const RequestType originalRequest);

  // returns the complete SIP message if successful.
  // this function will use information provided in above functions
  // to generate RFC 3261 compliant SIP message
  QString composeMessage(messageID id);

  void removeMessage(messageID id);

  // OPTIONAL ------------------------------

  QString formSDP(const std::shared_ptr<SDPMessageInfo> sdpInfo);
  void addSDP(messageID id, QString& sdp);

private:

  struct SIPMessage
  {
    // comments tell function which gives this value.
    QString method; // startSIP-function
    bool isRequest;
    QString version; // startSIP-function

    QString remoteName;     // to-function
    QString remoteUsername; // to-function
    QString remoteLocation; // to-function
    QString remoteTag;      // to-function

    QString maxForwards; // maxForwards-function

    QString localName;     // from-function
    QString localUsername; // from-function
    QString localLocation; // from-function
    QString localTag;      // from-function
    QString replyAddress; // via-function
    QString branch;       // via-function

    QString callID; // setCallID-function
    QString host;

    QString cSeq; // sequenceNumber-function
    QString originalRequest; // sequenceNumber-function

    QString contentType;   // addSDP-function
    QString contentLength; // addSDP-function
    QString content;       // addSDP-function
  };

  QString requestToString(const RequestType request);
  QString responseToString(const ResponseType response);

  bool checkMessageReady(SIPMessage* message);
  void initializeMessage(const QString& SIPversion);

  std::vector<SIPMessage*> messages_;
};
