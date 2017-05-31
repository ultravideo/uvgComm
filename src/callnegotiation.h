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


class CallNegotiation : public QObject
{
  Q_OBJECT
public:
  CallNegotiation();
  ~CallNegotiation();

  void init(QString localName, QString localUsername);
  void uninit();

  QList<QString> startCall(QList<Contact> addresses);
  void acceptCall(QString callID);
  void rejectCall(QString callID);
  void endCall(QString callID);

  void endAllCalls();

signals:

  // caller wants to establish a call.
  // Ask use if it is and call accept or reject call
  void incomingINVITE(QString CallID, QString caller);

  // we are calling ourselves.
  // TODO: Current implementation seizes the negotiation and just starts the call.
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

  // somebody has established a TCP connection with us.
  void receiveConnection(Connection* con);

  // We have received a message from one of our connection.
  void processMessage(QString header, QString content, quint32 connectionID);

  // connection has been established. This enables for us to get the needed info
  // to form a SIP message
  void connectionEstablished(quint32 connectionID);

private:

  struct SIPLink
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

  std::shared_ptr<SIPLink> newSIPLink();

  // checks that incoming SIP message makes sense
  bool compareSIPLinkInfo(std::shared_ptr<SIPMessageInfo> info, quint32 connectionID);

  // updates the Link information based on new message
  void newSIPLinkFromMessage(std::shared_ptr<SIPMessageInfo> info, quint32 connectionID);

  QString generateRandomString(uint32_t length);


  QList<QHostAddress> parseIPAddress(QString address);

  void processRequest(std::shared_ptr<SIPMessageInfo> info,
                      std::shared_ptr<SDPMessageInfo> peerSDP,
                      quint32 connectionID);

  void processResponse(std::shared_ptr<SIPMessageInfo> info,
                       std::shared_ptr<SDPMessageInfo> peerSDP);

  // checks that received SDP is what we are expecting.
  bool suitableSDP(std::shared_ptr<SDPMessageInfo> peerSDP);

  // helper function that composes SIP message and sends it
  void messageComposition(messageID id, std::shared_ptr<SIPLink> link);
  void sendRequest(RequestType request, std::shared_ptr<SIPLink> link);
  void sendResponse(ResponseType request, std::shared_ptr<SIPLink> link);

  void stopConnection(quint32 connectionID);
  void uninitSession(std::shared_ptr<SIPLink> link);

  QMutex sessionMutex_;
  // TODO: identify session with CallID AND tags, not just callID.
  // this enables splitting a call into multiple dialogs
  std::map<QString, std::shared_ptr<SIPLink>> sessions_;

  QMutex connectionMutex_;
  std::vector<Connection*> connections_;

  SIPStringComposer messageComposer_;

  uint16_t sipPort_;

  QString localName_;
  QString localUsername_;

  // listens to incoming tcp connections on sipPort_
  ConnectionServer server_;


  uint16_t firstAvailablePort_;
};
