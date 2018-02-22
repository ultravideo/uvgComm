#pragma once

#include "mediamanager.h"
#include "sipmanager.h"
#include "gui/callwindow.h"
#include "participantinterface.h"
#include "callcontrolinterface.h"
#include "stun.h"

#include <QObject>

struct SDPMessageInfo;
class StatisticsInterface;

class CallManager : public QObject, public ParticipantInterface, public CallControlInterface
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
  virtual void incomingCall(uint32_t sessionID, QString caller);
  virtual void callRinging(uint32_t sessionID);
  virtual void callRejected(uint32_t sessionID);
  virtual void callNegotiated(uint32_t sessionID, std::shared_ptr<SDPMessageInfo> peerInfo,
                              std::shared_ptr<SDPMessageInfo> localInfo);
  virtual void callNegotiationFailed(uint32_t sessionID);
  virtual void cancelIncomingCall(uint32_t sessionID);
  virtual void endCall(uint32_t sessionID, QString ip);
  virtual void registeredToServer();

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

  void createParticipant(uint32_t sessionID, std::shared_ptr<SDPMessageInfo> peerInfo,
                         const std::shared_ptr<SDPMessageInfo> localInfo,
                         VideoWidget *videoWidget,
                         StatisticsInterface* stats);

  MediaManager media_; // Media processing and delivery
  SIPManager sip_; // SIP
  CallWindow window_; // GUI

  StatisticsInterface* stats_;

  Stun stun_;
};
