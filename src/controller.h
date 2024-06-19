#pragma once

#include "media/mediamanager.h"
#include "initiation/sipmanager.h"
#include "ui/uimanager.h"
#include "participantinterface.h"
#include "mediaid.h"

#include <QObject>

#include <map>


/* This is the main class of the program and thus should control what, in which order
 * and when the other modules do tasks.
 */


struct SDPMessageInfo;
class StatisticsInterface;

class uvgCommController : public QObject, public ParticipantInterface
{
  Q_OBJECT
public:
  uvgCommController();

  void init();
  void uninit();

  // participant interface funtions used to start a call or a chat.
  uint32_t startINVITETransaction(QString name, QString username, QString ip);
  uint32_t chatWithParticipant(QString name, QString username, QString ip);

  // Call Control Interface used by SIP transaction
  bool processINVITE(uint32_t sessionID, QString caller);
  void processRINGING(uint32_t sessionID);
  void processINVITE_OK(uint32_t sessionID);
  void INVITETransactionConcluded(uint32_t sessionID);
  void cancelIncomingCall(uint32_t sessionID);
  void sessionTerminated(uint32_t sessionID);
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
  void quit();           // we should closed uvgComm
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
  void updateCallSettings();

  void negotiateNextCall();

  void connectionEstablished(QString localAddress, QString remoteAddress);

private:
  void removeSession(uint32_t sessionID, QString message, bool temporaryMessage);

  SIPConfig createSIPConfig();

  void createCall(uint32_t sessionID);

  void updateSDPAudioStatus(std::shared_ptr<SDPMessageInfo> sdp);
  void updateSDPVideoStatus(std::shared_ptr<SDPMessageInfo> sdp);

  void getRemoteSDP(uint32_t sessionID, std::shared_ptr<SIPMessageHeader> message,
                    QVariant& content);

  void renegotiateCall(uint32_t sessionID);
  void renegotiateAllCalls();

  void createSIPDialog(QString name, QString username, QString ip, uint32_t sessionID);

  void updateMediaIDs(uint32_t sessionID,
                      QList<MediaInfo>& localMedia,
                      QList<MediaInfo>& remoteMedia,
                      bool followOurSDP,
                      QList<std::pair<MediaID, MediaID>>& audioVideoIDs, QList<MediaID>& allIDs);

  void getMediaAttributes(const MediaInfo &local, const MediaInfo& remote,
                          bool followOurSDP, bool &send,
                          bool& receive);

  // the stun addresses are our outward network addresses which our peer uses to send data to us
  // we cannot however bind to those so we must translate the addresses to our local addresses
  void getStunBindings(uint32_t sessionID, MediaInfo &media);

  bool areWeFocus() const
  {
    return states_.size() > 1;
  }

  // this is a huge hack altogether
  bool areWeICEController(bool initialAgent, uint32_t sessionID) const;

  MediaID getMediaID(uint32_t sessionID, const MediaInfo &media);

  // call state is used to make sure everything is going according to plan,
  // no surprise ACK messages etc
  enum INVITETransactionState {
    CALL_INVITE_SENT,
    CALL_INVITE_RECEIVED,
    CALL_OK_SENT,
    CALL_TRANSACTION_CONCLUDED,
    CALL_ENDING
  };

  struct SessionState {
    INVITETransactionState state;
    std::shared_ptr<SDPMessageInfo> localSDP;
    std::shared_ptr<SDPMessageInfo> remoteSDP;
    bool followOurSDP;
    bool iceController;
    bool sessionNegotiated;
    bool sessionRunning;

    QString name;
  };

  std::map<uint32_t, SessionState> states_;

  std::deque<uint32_t> pendingRenegotiations_;

  MediaManager media_; // Media processing and delivery
  SIPManager sip_; // SIP
  UIManager userInterface_; // UI

  StatisticsInterface* stats_;

  QTimer delayAutoAccept_;
  uint32_t delayedAutoAccept_;

  int ongoingNegotiations_;

  QTimer delayedNegotiation_;

  std::map<uint32_t, std::shared_ptr<std::vector<MediaID>>> sessionMedias_;

  std::shared_ptr<VideoviewFactory> viewFactory_;

  // if we want to do something, but the TCP connection has not yet been established
  struct WaitingStart
  {
    QString realname;
    QString username;
    uint32_t sessionID;
  };

  std::map<QString, WaitingStart> waitingToStart_; // INVITE after connect
  QStringList waitingToBind_; // REGISTER after connect
};
