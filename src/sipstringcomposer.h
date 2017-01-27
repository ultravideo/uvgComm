#pragma once

#include <QHostInfo>
#include <QHostAddress>
#include <QString>

/* Handles creation of SIP requests and responses on at a time.
 * Will not check the legality of the request/response,
 * only that all required fields are provided.
 * Usage: call startRequest or startResponse */

enum MessageType {NOREQUEST, INVITE, ACK, BYE, CANCEL, OPTIONS, REGISTER, OK200}; // RFC 3261
              //PRACK,SUBSCRIBE, NOTIFY, PUBLISH, INFO, REFER, MESSAGE, UPDATE }; RFC 3262, 6665, 3903, 6086, 3515, 3428, 3311

typedef int16_t messageID;

class SIPStringComposer
{
public:
  SIPStringComposer();


  // MANDATORY ---------------------------

  messageID startSIPString(const MessageType message, const QString& SIPversion = "2.0");

  // include their tag only if it was already provided
  void to(messageID id, QString& name, QString& username, const QHostInfo& hostname, const QString& tag = "");
  void toIP(messageID id, QString& name, QString& username, const QHostAddress& address, const QString& tag = "");

  void from(messageID id, QString& name, QString& username, const QHostInfo& hostname, const QString& tag);
  void fromIP(messageID id, QString& name, QString& username, const QHostAddress& address, const QString& tag);

  // Where to send responses. branch is generated. Also adds the contact field with same info.
  void via(messageID id, const QHostInfo& hostname, QString& branch);
  void viaIP(messageID id, QHostAddress address, QString& branch);

  void maxForwards(messageID id, uint32_t forwards);

  // string must include host!
  void setCallID(messageID id, QString& callID);

  void sequenceNum(messageID id, uint32_t seq);

  // returns the complete SIP message if successful.
  // this function will use information provided in above functions
  // to generate RFC 3261 compliant SIP message
  QString composeMessage(messageID id);

  void removeMessage(messageID id);

  // OPTIONAL ------------------------------

  void addSDP(messageID id, QString& sdp);

private:

  struct SIPMessage
  {
    // comments tell function which gives this value.
    QString request; // startSIPString-function
    QString version; // startSIPString-function

    QString theirName;     // to-function
    QString theirUsername; // to-function
    QString theirLocation; // to-function
    QString theirTag;      // to-function

    QString maxForwards; // maxForwards-function

    QString ourName;     // from-function
    QString ourUsername; // from-function
    QString ourLocation; // from-function
    QString ourTag;      // from-function
    QString replyAddress; // via-function
    QString branch;       // via-function

    QString callID; // setCallID-function

    QString cSeq; // sequenceNumber-function

    QString contentType;   // addSDP-function
    QString contentLength; // addSDP-function
    QString content;       // addSDP-function
  };

  bool checkMessageReady(SIPMessage* message);

  std::vector<SIPMessage*> messages_;
};
