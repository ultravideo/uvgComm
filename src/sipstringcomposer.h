#pragma once

#include <QHostAddress>
#include <QString>


/* Handles creation of SIP requests and responses on at a time.
 * Will not check the legality of the request/response,
 * only that all required fields are provided.
 * Usage: call startRequest or startResponse */

enum Request {NOREQUEST, INVITE, ACK, BYE, CANCEL, OPTIONS, REGISTER}; // RFC 3261
              //PRACK,SUBSCRIBE, NOTIFY, PUBLISH, INFO, REFER, MESSAGE, UPDATE }; RFC 3262, 6665, 3903, 6086, 3515, 3428, 3311

enum Response {OK200};

typedef int16_t messageID;

class SIPStringComposer
{
public:
  SIPStringComposer();

  messageID startRequest(const Request request, const QString& SIPversion = "2.0");
  messageID startResponse(const Response reponse, const QString& SIPversion = "2.0");

  // does not add tag if not given. Adding a new tag is the responsibility of the other end
  void to(messageID id, const QString& name, QString& username, const QString& hostname, const QString& tag = "");
  void toIP(messageID id, const QString& name, QString& username, QHostAddress& address, const QString& tag = "");

  // adds tag if not given.
  void from(messageID id, const QString& name, QString& username, const QString& hostname, const QString& tag = "");
  void fromIP(messageID id, const QString& name, QString& username, QHostAddress& address, const QString& tag = "");

  // Where to send responses. branch is generated. Also adds the contact field with same info.
  void via(messageID id, const QString& hostname);
  void viaIP(messageID id, QHostAddress address);

  void maxForwards(messageID id, uint32_t forwards);

  // if not called a new one is generated
  void setCallID(messageID id, QString& callID);

  void sequenceNum(messageID id, uint32_t seq);

  void addSDP(messageID id, QString& sdp);

  // returns the complete SIP message if successful.
  QString& composeMessage(messageID id);

  void removeMessage(messageID id);

private:

  struct SIPMessage
  {
    // comments tell function which gives this value. Do not include field names.
    QString request; // start-function

    QString theirName; // to-function
    QString theirUsername; // to-function
    QString theirLocation; // to-function
    QString theirTag;

    QString maxForwards; // maxForwards-function

    QString myName; // from-function
    QString myUsername; // from-function
    QString myLocation; // from-function
    QString replyAddress; // via-function
    QString myTag;

    QString callID; // setCallID-function

    QString cSeq; // sequenceNumber-function

    QString contentType; // addSDP-function
    QString contentLength; // addSDP-function
    QString content; // addSDP-function

    QString branch; // generated
  };


  std::vector<SIPMessage*> messages_;
};
