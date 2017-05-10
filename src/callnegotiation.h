#pragma once



#include "connectionserver.h"
#include "connection.h"
#include "sipstringcomposer.h"
#include "common.h"

#include <MediaSession.hh>

#include <QHostAddress>
#include <QString>

#include <memory>

struct SIPMessageInfo;

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

  void init(QString localName);
  void uninit();

  void setupSession(MediaSubsession* subsession);

  QList<QString> startCall(QList<Contact> addresses);
  void acceptCall(QString callID);
  void rejectCall(QString callID);
  void endCall(QString callID);

  void endAllCalls();

signals:

  // caller wants to call us. Ask the user whether call is accepted
  void incomingINVITE(QString CallID, QString caller);
  void callingOurselves(QString callID, std::shared_ptr<SDPMessageInfo> info);
  void callNegotiated(QString callID, std::shared_ptr<SDPMessageInfo> peerInfo,
                      std::shared_ptr<SDPMessageInfo> localInfo);

  void ringing(QString callID);
  void ourCallAccepted(QString callID, std::shared_ptr<SDPMessageInfo> peerInfo,
                       std::shared_ptr<SDPMessageInfo> localInfo);
  void ourCallRejected(QString CallID);

  void callEnded(QString callID, QString ip);

private slots:
  void receiveConnection(Connection* con);
  void processMessage(QString header, QString content, quint32 connectionID);

  void connectionEstablished(quint32 connectionID);

private:

  struct SIPLink
  {
    QString callID; // for identification
    QString host;

    Contact contact;

    QString replyAddress;
    //uint32_t cSeq; // TODO cSeq is only the number of forwards!!

    QString ourTag;
    QString theirTag;
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
  bool compareSIPLinkInfo(std::shared_ptr<SIPMessageInfo> info, quint32 connectionID);
  void newSIPLinkFromMessage(std::shared_ptr<SIPMessageInfo> info, quint32 connectionID);

  QString generateRandomString(uint32_t length);

  QList<QHostAddress> parseIPAddress(QString address);

  void processRequest(std::shared_ptr<SIPMessageInfo> info,
                      std::shared_ptr<SDPMessageInfo> peerSDP);

  void processResponse(std::shared_ptr<SIPMessageInfo> info,
                       std::shared_ptr<SDPMessageInfo> peerSDP);

  bool suitableSDP(std::shared_ptr<SDPMessageInfo> peerSDP);

  // helper function that composes SIP message and sends it
  void messageComposition(messageID id, std::shared_ptr<SIPLink> link);
  void sendRequest(RequestType request, std::shared_ptr<SIPLink> link);
  void sendResponse(ResponseType request, std::shared_ptr<SIPLink> link);

  void uninitSession(std::shared_ptr<SIPLink> link);

  std::map<QString, std::shared_ptr<SIPLink>> sessions_;

  QMutex connectionMutex_;
  std::vector<Connection*> connections_;

  SIPStringComposer messageComposer_;

  uint16_t sipPort_;

  QString ourName_;
  QString ourUsername_;
  //QHostAddress ourLocation_;

  // listens to incoming tcp connections on sipPort_
  ConnectionServer server_;


  uint16_t firstAvailablePort_;
};
