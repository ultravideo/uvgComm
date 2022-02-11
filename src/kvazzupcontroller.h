#pragma once

#include "media/mediamanager.h"
#include "initiation/sipmanager.h"
#include "initiation/siptransactionuser.h"
#include "ui/uimanager.h"
#include "participantinterface.h"

#include <QObject>

#include <map>


/* This is the main class of the program and thus should control what, in which order
 * and when the other modules do tasks.
 */


struct SDPMessageInfo;
class StatisticsInterface;

class KvazzupController :
    public QObject,
    public ParticipantInterface,
    public SIPTransactionUser
{
  Q_OBJECT
public:
  KvazzupController();

  void init();
  void uninit();

  // participant interface funtions used to start a call or a chat.
  virtual uint32_t callToParticipant(QString name, QString username, QString ip);
  virtual uint32_t chatWithParticipant(QString name, QString username, QString ip);

  // Call Control Interface used by SIP transaction
  virtual bool incomingCall(uint32_t sessionID, QString caller);
  virtual void callRinging(uint32_t sessionID);
  virtual void peerAccepted(uint32_t sessionID);
  virtual void callNegotiated(uint32_t sessionID);
  virtual void cancelIncomingCall(uint32_t sessionID);
  virtual void endCall(uint32_t sessionID);
  virtual void failure(uint32_t sessionID, QString error);
  virtual void registeredToServer();
  virtual void registeringFailed();

  void SIPRequestCallback(uint32_t sessionID,
                          SIPRequest& request,
                          QVariant& content);

  void SIPResponseCallback(uint32_t sessionID,
                           SIPResponse& response,
                           QVariant& content);

  void processRegisterRequest(QString address,
                              SIPRequest& request,
                              QVariant& content);

  void processRegisterResponse(QString address,
                               SIPResponse& response,
                               QVariant& content);

public slots:  

  // reaction to user UI interactions
  void endTheCall();     // user wants to end the call
  void quit();           // we should closed Kvazzup
  void userAcceptsCall(uint32_t sessionID); // user has accepted the incoming call
  void userRejectsCall(uint32_t sessionID); // user has rejected the incoming call
  void userCancelsCall(uint32_t sessionID); // user has rejected the incoming call

  void iceCompleted(quint32 sessionID,
                    const std::shared_ptr<SDPMessageInfo> local,
                    const std::shared_ptr<SDPMessageInfo> remote);
  void iceFailed(quint32 sessionID);

  void zrtpFailed(quint32 sessionID);

  void noEncryptionAvailable();

  void delayedAutoAccept();
private:
  void removeSession(uint32_t sessionID, QString message, bool temporaryMessage);

  void createSingleCall(uint32_t sessionID);
  void setupConference();

  // Call state is a non-dependant way
  enum CallState {
    CALLRINGINGWITHUS,
    CALLINGTHEM,
    CALLRINGINWITHTHEM,
    CALLNEGOTIATING,
    CALLWAITINGICE,
    CALLONGOING
  };

  struct SessionState {
    CallState state;
    std::shared_ptr<SDPMessageInfo> localSDP;
    std::shared_ptr<SDPMessageInfo> remoteSDP;
  };

  std::map<uint32_t, SessionState> states_;

  MediaManager media_; // Media processing and delivery
  SIPManager sip_; // SIP
  UIManager userInterface_; // UI

  StatisticsInterface* stats_;

  QTimer delayAutoAccept_;
  uint32_t delayedAutoAccept_;
};
