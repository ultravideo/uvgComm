#pragma once

#include "media/mediamanager.h"
#include "initiation/sipmanager.h"
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
    public ParticipantInterface
{
  Q_OBJECT
public:
  KvazzupController();

  void init();
  void uninit();

  // participant interface funtions used to start a call or a chat.
  uint32_t callToParticipant(QString name, QString username, QString ip);
  uint32_t chatWithParticipant(QString name, QString username, QString ip);

  // Call Control Interface used by SIP transaction
  bool incomingCall(uint32_t sessionID, QString caller);
  void callRinging(uint32_t sessionID);
  void peerAccepted(uint32_t sessionID);
  void callNegotiated(uint32_t sessionID);
  void cancelIncomingCall(uint32_t sessionID);
  void endCall(uint32_t sessionID);
  void failure(uint32_t sessionID, QString error);
  void registeredToServer();
  void registeringFailed();

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

  void inputLocalSDP(uint32_t sessionID, std::shared_ptr<SDPMessageInfo> local);
  void iceFailed(quint32 sessionID);

  void zrtpFailed(quint32 sessionID);

  void noEncryptionAvailable();

  void delayedAutoAccept();

  void updateAudioSettings();
  void updateVideoSettings();
private:
  void removeSession(uint32_t sessionID, QString message, bool temporaryMessage);

  void createCall(uint32_t sessionID);
  void setupConference();

  void updateSDPAudioStatus(std::shared_ptr<SDPMessageInfo> sdp);
  void updateSDPVideoStatus(std::shared_ptr<SDPMessageInfo> sdp);

  void getRemoteSDP(uint32_t sessionID, std::shared_ptr<SIPMessageHeader> message,
                    QVariant& content);

  void getReceiveAttribute(std::shared_ptr<SDPMessageInfo> sdp, bool isThisLocal,
                           bool& recvVideo, bool& recvAudio);

  // call state is used to make sure everything is going according to plan,
  // no surprise ACK messages etc
  enum CallState {
    CALLNOSTATE,
    CALLRINGINGWITHUS,
    CALLINGTHEM,
    CALLRINGINWITHTHEM,
    CALLNEGOTIATING,
    CALLONGOING,
    CALLENDING
  };

  struct SessionState {
    CallState state;
    std::shared_ptr<SDPMessageInfo> localSDP;
    std::shared_ptr<SDPMessageInfo> remoteSDP;
    bool followOurSDP;

    QString name;
  };

  std::map<uint32_t, SessionState> states_;

  MediaManager media_; // Media processing and delivery
  SIPManager sip_; // SIP
  UIManager userInterface_; // UI

  StatisticsInterface* stats_;

  QTimer delayAutoAccept_;
  uint32_t delayedAutoAccept_;

  // video views
  std::shared_ptr<VideoviewFactory> viewFactory_;
};
