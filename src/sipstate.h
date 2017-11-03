#pragma once
#include "connectionserver.h"
#include "sipstringcomposer.h"

#include <MediaSession.hh>

#include <QHostAddress>
#include <QString>
#include <QMutex>

#include <memory>

struct SIPMessageInfo;
class Connection;

struct Contact
{
  QString name;
  QString username;
  QString contactAddress;
};


class SIPState : public QObject
{
  Q_OBJECT
public:
  SIPState();
  ~SIPState();

  void init();
  void uninit();

  QString startCall(Contact address);
  void acceptCall();
  void rejectCall();
  void endCall();

  void setPeerConnection(QString ourAddress, QString theirAddress);
  void setServerConnection(QString hostAddress);

signals:

  // caller wants to establish a call.
  // Ask use if it is and call accept or reject call
  void incomingINVITE(QString caller);

  // we are calling ourselves.
  // TODO: Current implementation ceases the negotiation and just starts the call.
  void callingOurselves(QString callID);

  // their call which we have accepted has finished negotiating. Call can now start.
  void callNegotiated(QString callID);

  // Local call is waiting for user input at remote end
  void ringing(QString callID);

  // Call iniated locally has been accepted by peer. Call can now start.
  void ourCallAccepted(QString callID);

  // Remote rejected local INVITE
  void ourCallRejected(QString CallID);

  // Received call ending signal (BYE)
  void callEnded(QString callID, QString ip);

private slots:

  // We have received a message from one of our connection.
  void processMessage(QString header, QString content, quint32 connectionID);

private:

  struct SIPSessionInfo
  {
    QString callID; // for identification
    QString host;

    Contact contact;

    QString replyAddress;
    //uint32_t cSeq; // TODO cSeq is only the number of forwards!!

    QString localTag;
    QString remoteTag;
    QHostAddress localAddress;

    uint32_t connectionID;

    RequestType originalRequest;

    QList<uint16_t> suggestedPorts_;
  };

  std::shared_ptr<SIPSessionInfo> newSIPSessionInfo();

  // checks that incoming SIP message makes sense
  bool compareSIPSessionInfo(std::shared_ptr<SIPMessageInfo> mInfo, quint32 connectionID);

  // updates the info information based on new message
  void newSIPSessionInfoFromMessage(std::shared_ptr<SIPMessageInfo> mInfo, quint32 connectionID);

  QString generateRandomString(uint32_t length);


  QList<QHostAddress> parseIPAddress(QString address);

  void processRequest(std::shared_ptr<SIPMessageInfo> mInfo,
                      quint32 connectionID);

  void processResponse(std::shared_ptr<SIPMessageInfo> mInfo);

  // helper function that composes SIP message and sends it
  QString messageComposition(messageID id, std::shared_ptr<SIPSessionInfo> info);
  void sendRequest(RequestType request, std::shared_ptr<SIPSessionInfo> info);
  void sendResponse(ResponseType request, std::shared_ptr<SIPSessionInfo> info);

  QMutex sessionMutex_;
  std::shared_ptr<SIPSessionInfo> session_;

  SIPStringComposer messageComposer_;

  QString localName_;
  QString localUsername_;
};
