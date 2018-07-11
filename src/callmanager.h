#pragma once

#include "mediamanager.h"
#include "sip/siptransactions.h"
#include "gui/callwindow.h"
#include "participantinterface.h"
#include "sip/siptransactionuser.h"
#include "stun.h"

#include <QObject>

struct SDPMessageInfo;
class StatisticsInterface;

class CallManager : public QObject, public ParticipantInterface, public SIPTransactionUser
{
  Q_OBJECT
public:
  CallManager();

  void init();
  void uninit();

  // participant interface funtions used to start a call or a chat.
  virtual void callToParticipant(QString name, QString username, QString ip);
  virtual void chatWithParticipant(QString name, QString username, QString ip);

  // Call Control Interface userd by SIP
  virtual bool incomingCall(uint32_t sessionID, QString caller);
  virtual void callRinging(uint32_t sessionID);
  virtual void callRejected(uint32_t sessionID);
  virtual void callNegotiated(uint32_t sessionID);
  virtual void callNegotiationFailed(uint32_t sessionID);
  virtual void cancelIncomingCall(uint32_t sessionID);
  virtual void endCall(uint32_t sessionID);
  virtual void registeredToServer();
  virtual void registeringFailed();

public slots:  

  // reaction to user GUI interactions
  void updateSettings();
  void micState();
  void cameraState();
  void endTheCall();
  void windowClosed();
  void acceptCall(uint32_t sessionID);
  void rejectCall(uint32_t sessionID);

private slots:

  void stunAddress(QHostAddress message);
  void noStunAddress();

private:

  // TODO: Move this to media manager.
  void createParticipant(uint32_t sessionID, std::shared_ptr<SDPMessageInfo> peerInfo,
                         const std::shared_ptr<SDPMessageInfo> localInfo,
                         StatisticsInterface* stats);

  struct PeerState
  {
    bool viewAdded;
    bool mediaAdded;
  };

  MediaManager media_; // Media processing and delivery
  SIPTransactions sip_; // SIP
  CallWindow window_; // GUI

  StatisticsInterface* stats_;

  Stun stun_;


};
