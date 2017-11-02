#pragma once
#include "connectionserver.h"
#include "sipstringcomposer.h"

#include <MediaSession.hh>

#include <QHostAddress>
#include <QString>
#include <QMutex>

#include <memory>

struct SIPMessageInfo;
struct SDPMessageInfo;
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

  void endAllCalls();

  void setPeerConnection(QString ourAddress, QString theirAddress);
  void setServerConnection(QString hostAddress);

signals:

  // caller wants to establish a call.
  // Ask use if it is and call accept or reject call
  void incomingINVITE(QString CallID, QString caller);

  // we are calling ourselves.
  // TODO: Current implementation ceases the negotiation and just starts the call.
  void callingOurselves(QString callID, std::shared_ptr<SDPMessageInfo> info);

  // their call which we have accepted has finished negotiating. Call can now start.
  void callNegotiated(QString callID, std::shared_ptr<SDPMessageInfo> peerInfo,
                      std::shared_ptr<SDPMessageInfo> localInfo);

  // Local call is waiting for user input at remote end
  void ringing(QString callID);

  // Call iniated locally has been accepted by peer. Call can now start.
  void ourCallAccepted(QString callID, std::shared_ptr<SDPMessageInfo> peerInfo,
                       std::shared_ptr<SDPMessageInfo> localInfo);

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

    std::shared_ptr<SDPMessageInfo> localSDP;
    std::shared_ptr<SDPMessageInfo> peerSDP;

    uint32_t connectionID;

    RequestType originalRequest;

    QList<uint16_t> suggestedPorts_;

    bool finalSDP;
  };

  std::shared_ptr<SDPMessageInfo> generateSDP(QString localAddress);

  std::shared_ptr<SIPSessionInfo> newSIPSessionInfo();

  // checks that incoming SIP message makes sense
  bool compareSIPSessionInfo(std::shared_ptr<SIPMessageInfo> mInfo, quint32 connectionID);

  // updates the info information based on new message
  void newSIPSessionInfoFromMessage(std::shared_ptr<SIPMessageInfo> mInfo, quint32 connectionID);

  QString generateRandomString(uint32_t length);


  QList<QHostAddress> parseIPAddress(QString address);

  void processRequest(std::shared_ptr<SIPMessageInfo> mInfo,
                      std::shared_ptr<SDPMessageInfo> peerSDP,
                      quint32 connectionID);

  void processResponse(std::shared_ptr<SIPMessageInfo> mInfo,
                       std::shared_ptr<SDPMessageInfo> peerSDP);

  // checks that received SDP is what we are expecting.
  bool suitableSDP(std::shared_ptr<SDPMessageInfo> peerSDP);

  // helper function that composes SIP message and sends it
  void messageComposition(messageID id, std::shared_ptr<SIPSessionInfo> info);
  void sendRequest(RequestType request, std::shared_ptr<SIPSessionInfo> info);
  void sendResponse(ResponseType request, std::shared_ptr<SIPSessionInfo> info);

  void stopConnection(quint32 connectionID);
  void uninitSession(std::shared_ptr<SIPSessionInfo> info);

  QMutex sessionMutex_;
  // TODO: identify session with CallID AND tags, not just callID.
  // this enables splitting a call into multiple dialogs
  //std::map<QString, std::shared_ptr<SIPSessionInfo>> sessions_;
  std::shared_ptr<SIPSessionInfo> session_;

  QMutex connectionMutex_;
  std::vector<Connection*> connections_;

  SIPStringComposer messageComposer_;

  QString localName_;
  QString localUsername_;

  uint16_t firstAvailablePort_;
};
