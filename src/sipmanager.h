#pragma once

#include "globalsdpstate.h"
#include "sipconnection.h"
#include "connectionserver.h"

#include "common.h"

/* This class manages the interactions between different components in
 * a SIP transaction. This class should implement as little functionality
 * as possible.
 */

struct Contact
{
  QString username;
  QString realName;
  QString remoteAddress;
};

class SIPRouting;
class SIPSession;
class CallControlInterface;

class SIPManager : public QObject
{
  Q_OBJECT
public:
  SIPManager();

  void init(CallControlInterface* callControl);
  void uninit();

  QList<uint32_t> startCall(QList<Contact> addresses);
  void acceptCall(uint32_t sessionID);
  void rejectCall(uint32_t sessionID);
  void endCall(uint32_t sessionID);

  void endAllCalls();

  void makeConference();
  void dispandConference();

private slots:
  // connection has been established. This enables for us to get the needed info
  // to form a SIP message
  void connectionEstablished(quint32 sessionID, QString localAddress, QString remoteAddress);
  void receiveTCPConnection(TCPConnection* con);

  void processSIPRequest(SIPRequest request, quint32 sessionID);
  void processSIPResponse(SIPResponse response, quint32 sessionID);

  void sendRequest(uint32_t sessionID, RequestType type);
  void sendResponse(uint32_t sessionID, ResponseType response);

private:
  struct SIPDialogData
  {
    std::shared_ptr<SIPConnection> sCon;
    std::shared_ptr<SIPSession> session;
    std::shared_ptr<SIPRouting> routing;
    QString remoteUsername;
    // has local invite sdp or o response sdp
    std::shared_ptr<SDPMessageInfo> localSdp_;
    // empty until final ok 200
    std::shared_ptr<SDPMessageInfo> remoteSdp_;
  };

  std::shared_ptr<SIPSession> createSIPSession(uint32_t sessionID);
  std::shared_ptr<SIPConnection> createSIPConnection();
  std::shared_ptr<SIPRouting> createSIPRouting(QString remoteUsername,
                                               QString localAddress,
                                               QString remoteAddress, bool hostedSession);

  void destroyDialog(std::shared_ptr<SIPDialogData> dialog);

  // tmp function to convert new structs to old
  void toSIPMessageInfo(SIPRoutingInfo info);

  // This mutex makes sure that the dialog has been added to the dialogs_ list
  // before we are accessing it when receiving messages
  QMutex connectionMutex_;

  // sessionID:s are positions in this list. SessionID:s are used in this program to
  // keep track of sessions. The CallID is not used because we could be calling ourselves
  // and using uint32_t is simpler than keeping track of tags

  QList<std::shared_ptr<SIPDialogData>> dialogs_;

  bool isConference_;

  GlobalSDPState sdp_;

  ConnectionServer server_;
  uint16_t sipPort_;

  QString localName_;
  QString localUsername_;

  CallControlInterface* callControl_;
};
