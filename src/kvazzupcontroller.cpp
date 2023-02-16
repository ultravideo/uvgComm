#include "kvazzupcontroller.h"

#include "statisticsinterface.h"
#include "videoviewfactory.h"

#include "common.h"
#include "settingskeys.h"
#include "logger.h"

#include <QSettings>
#include <QHostAddress>

#include <thread>
#include <chrono>

// TODO: Eliminating this would speed up the start of conference, but does require making the media more
// resilient to repeated re-negotiations
const int DELAYED_NEGOTIATION_TIMEOUT_MS = 500;


KvazzupController::KvazzupController():
  states_(),
  media_(),
  sip_(),
  userInterface_(),
  stats_(nullptr),
  delayAutoAccept_(),
  delayedAutoAccept_(0),
  ongoingNegotiations_(0),
  delayedNegotiation_(),
  viewFactory_(std::shared_ptr<VideoviewFactory>(new VideoviewFactory()))
{}


void KvazzupController::init()
{
  Logger::getLogger()->printImportant(this, "Kvazzup initiation Started");

  userInterface_.init(this);

  viewFactory_->addSelfview(userInterface_.getSelfView());

  stats_ = userInterface_.createStatsWindow();

  QObject::connect(&sip_, &SIPManager::finalLocalSDP,
                   this, &KvazzupController::inputLocalSDP);

  QObject::connect(&sip_, &SIPManager::connectionEstablished,
                   this, &KvazzupController::connectionEstablished);


  sip_.installSIPRequestCallback(std::bind(&KvazzupController::SIPRequestCallback, this,
                                           std::placeholders::_1,
                                           std::placeholders::_2,
                                           std::placeholders::_3));
  sip_.installSIPResponseCallback(std::bind(&KvazzupController::SIPResponseCallback, this,
                                            std::placeholders::_1,
                                            std::placeholders::_2,
                                            std::placeholders::_3));

  sip_.installSIPRequestCallback(std::bind(&KvazzupController::processRegisterRequest, this,
                                           std::placeholders::_1,
                                           std::placeholders::_2,
                                           std::placeholders::_3));
  sip_.installSIPResponseCallback(std::bind(&KvazzupController::processRegisterResponse, this,
                                            std::placeholders::_1,
                                            std::placeholders::_2,
                                            std::placeholders::_3));

  std::shared_ptr<SDPMessageInfo> sdp = sip_.generateSDP(getLocalUsername(), 1, 1,
                                                         {"opus"}, {"H265"}, {0}, {});

  updateSDPAudioStatus(sdp);
  updateSDPVideoStatus(sdp);

  sip_.setSDP(sdp);

  sip_.init(stats_);
  sip_.listenToAny(SIP_TCP, 5060);

  checkBinding();

  QObject::connect(&media_, &MediaManager::handleZRTPFailure,
                   this,    &KvazzupController::zrtpFailed);

  QObject::connect(&media_, &MediaManager::handleNoEncryption,
                   this,    &KvazzupController::noEncryptionAvailable);

  // connect sip signal so we get information when ice fails
  QObject::connect(&media_, &MediaManager::iceMediaFailed,
                   this, &KvazzupController::iceFailed);

  media_.init(viewFactory_->getSelfVideos(), stats_);

  // register the GUI signals indicating GUI changes to be handled
  // approrietly in a system wide manner
  QObject::connect(&userInterface_, &UIManager::updateCallSettings,
                   &sip_, &SIPManager::updateCallSettings);

  QObject::connect(&userInterface_, &UIManager::updateVideoSettings,
                   this, &KvazzupController::updateVideoSettings);
  QObject::connect(&userInterface_, &UIManager::updateAudioSettings,
                   this, &KvazzupController::updateAudioSettings);

  QObject::connect(&userInterface_, &UIManager::updateAutomaticSettings,
                   &media_, &MediaManager::updateAutomaticSettings);

  QObject::connect(&userInterface_, &UIManager::endCall, this, &KvazzupController::endTheCall);
  QObject::connect(&userInterface_, &UIManager::quit, this, &KvazzupController::quit);

  QObject::connect(&userInterface_, &UIManager::callAccepted,
                   this, &KvazzupController::userAcceptsCall);
  QObject::connect(&userInterface_, &UIManager::callRejected,
                   this, &KvazzupController::userRejectsCall);
  QObject::connect(&userInterface_, &UIManager::callCancelled,
                   this, &KvazzupController::userCancelsCall);

  // lastly, show the window when our signals are ready
  userInterface_.showMainWindow();

  Logger::getLogger()->printImportant(this, "Kvazzup initiation finished");
}


void KvazzupController::uninit()
{
  // for politeness, we send BYE messages to all our call participants.
  endTheCall();
  sip_.uninit();
  media_.uninit();
}


void KvazzupController::quit()
{
  uninit();
}


uint32_t KvazzupController::startINVITETransaction(QString name, QString username,
                                                   QString ip)
{
  uint32_t sessionID = sip_.reserveSessionID();
  waitingToStart_[ip] = {name, username, sessionID};

  // try connecting, if returns immediately or we already have a connection, create Dialog
  if (sip_.connect(SIP_TCP, ip, 5060))
  {
    waitingToStart_.erase(ip);
    createSIPDialog(name, username, ip, sessionID);
  }

  return sessionID;
}


void KvazzupController::createSIPDialog(QString name, QString username, QString ip, uint32_t sessionID)
{
  NameAddr remote;
  remote.realname = name;
  remote.uri.type = DEFAULT_SIP_TYPE;
  remote.uri.hostport.host = ip;
  remote.uri.hostport.port = 0;
  remote.uri.userinfo.user = username;

  //start negotiations for this connection

  Logger::getLogger()->printNormal(this, "Starting call with contact",
                                   {"Contact"}, {remote.realname});

  sip_.createDialog(sessionID, remote, ip);

  userInterface_.displayOutgoingCall(sessionID, remote.realname);
  if(states_.find(sessionID) == states_.end())
  {
    states_[sessionID] = SessionState{CALL_INVITE_SENT, nullptr, nullptr, true, false, false, name};
  }
  else
  {
    Logger::getLogger()->printProgramError(this, "SIP Manager is giving existing sessionIDs as new ones");
  }

  // first we must renegotiates this call, so we get all their media parameters
  renegotiateCall(sessionID);

  // then we renegotiates rest of the calls with the previous calls parameters included
  for (auto& state : states_)
  {
    if (state.first != sessionID)
    {
      renegotiateCall(state.first);
    }
  }

  if (ongoingNegotiations_ == 0)
  {
    negotiateNextCall();
  }
}


uint32_t KvazzupController::chatWithParticipant(QString name, QString username,
                                                QString ip)
{
  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Starting a chat with contact",
            {"ip", "Name", "Username"}, {ip, name, username});
  return 0;
}


bool KvazzupController::processINVITE(uint32_t sessionID, QString caller)
{
  if(states_.find(sessionID) != states_.end())
  {
    Logger::getLogger()->printProgramError(this, "Incoming call is overwriting "
                                                 "an existing session!");
  }

  states_[sessionID] = SessionState{CALL_INVITE_RECEIVED, nullptr, nullptr, false, false, false, caller};
  ++ongoingNegotiations_;

  if(settingEnabled(SettingsKey::localAutoAccept))
  {
    Logger::getLogger()->printNormal(this, "Incoming call auto-accepted");

    // make sure there are no ongoing auto-accepts
    while (delayedAutoAccept_ != 0)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    delayedAutoAccept_ = sessionID;
    delayAutoAccept_.singleShot(100, this, &KvazzupController::delayedAutoAccept);
    return false;
  }
  else
  {
    Logger::getLogger()->printNormal(this, "Showing incoming call");
    userInterface_.displayIncomingCall(sessionID, caller);
  }
  return false;
}


void KvazzupController::delayedAutoAccept()
{
  userAcceptsCall(delayedAutoAccept_);
  delayedAutoAccept_ = 0;
}


void KvazzupController::updateAudioSettings()
{
  std::shared_ptr<SDPMessageInfo> sdp = sip_.generateSDP(getLocalUsername(), 1, 1,
                                                         {"opus"}, {"H265"}, {0}, {});
  updateSDPVideoStatus(sdp);
  updateSDPAudioStatus(sdp);

  emit media_.updateAudioSettings();

  sip_.setSDP(sdp);
  renegotiateAllCalls();
  if (ongoingNegotiations_ == 0)
  {
    negotiateNextCall();
  }
}


void KvazzupController::updateVideoSettings()
{
  std::shared_ptr<SDPMessageInfo> sdp = sip_.generateSDP(getLocalUsername(), 1, 1,
                                                         {"opus"}, {"H265"}, {0}, {});

  updateSDPVideoStatus(sdp);
  updateSDPAudioStatus(sdp);

  // TODO: For everything that is negotiated via SDP, the media should not be modified here

  // NOTE: Media must be updated before SIP so that SIP does not time-out while we update media
  emit media_.updateVideoSettings();
  sip_.setSDP(sdp);
  renegotiateAllCalls();
  if (ongoingNegotiations_ == 0)
  {
    negotiateNextCall();
  }
}


void KvazzupController::updateCallSettings()
{
  checkBinding();
  sip_.updateCallSettings();
}


void KvazzupController::updateSDPAudioStatus(std::shared_ptr<SDPMessageInfo> sdp)
{
  if (!settingEnabled(SettingsKey::micStatus))
  {
    for (auto& media : sdp->media)
    {
      if (media.type == "audio")
      {
        media.flagAttributes = {A_RECVONLY};
      }
    }
  }
}


void KvazzupController::updateSDPVideoStatus(std::shared_ptr<SDPMessageInfo> sdp)
{
  if (!settingEnabled(SettingsKey::screenShareStatus) &&
      !settingEnabled(SettingsKey::cameraStatus))
  {
    for (auto& media : sdp->media)
    {
      if (media.type == "video")
      {
        media.flagAttributes = {A_RECVONLY};
      }
    }
  }
}


void KvazzupController::processRINGING(uint32_t sessionID)
{
  // TODO_RFC 3261: Enable cancelling the request at this point
  // to make sure the original request has been received

  if(states_.find(sessionID) != states_.end() &&
     states_[sessionID].state == CALL_INVITE_SENT)
  {
    Logger::getLogger()->printNormal(this, "Our call is ringing");
    userInterface_.displayRinging(sessionID);
  }
  else
  {
    Logger::getLogger()->printPeerError(this, "Got call ringing for nonexisting call",
                  {"SessionID"}, {QString::number(sessionID)});
  }
}


void KvazzupController::processINVITE_OK(uint32_t sessionID)
{
  if(states_.find(sessionID) != states_.end())
  {
    if(states_[sessionID].state == CALL_INVITE_SENT)
    {
      Logger::getLogger()->printImportant(this, "They accepted our call!");
      INVITETransactionConcluded(sessionID);
    }
    else
    {
      Logger::getLogger()->printPeerError(this, "Got an accepted call even though "
                                                "we have not yet called them!",
                                          "sessionID", QString::number(sessionID));
    }
  }
  else
  {
    Logger::getLogger()->printPeerError(this, "Peer accepted a session which is not in Controller.");
  }
}


void KvazzupController::INVITETransactionConcluded(uint32_t sessionID)
{
  Logger::getLogger()->printNormal(this, "Call negotiated", "SessionID", QString::number(sessionID));

  if (states_.find(sessionID) != states_.end())
  {
    states_[sessionID].state = CALL_TRANSACTION_CONCLUDED; // all messages received for call
    states_[sessionID].sessionNegotiated = true;

    // we start the session only if we have both SDP messages
    if (states_[sessionID].localSDP != nullptr &&
        states_[sessionID].remoteSDP != nullptr)
    {
       --ongoingNegotiations_;
      Logger::getLogger()->printImportant(this, "Starting session for peer",
                  "SessionID", {QString::number(sessionID)});

      createCall(sessionID);

      if (!pendingRenegotiations_.empty())
      {
        delayedNegotiation_.singleShot(DELAYED_NEGOTIATION_TIMEOUT_MS,
                                       this, &KvazzupController::negotiateNextCall);
      }
    }
    else
    {
      Logger::getLogger()->printNormal(this, "Still waiting for SDP messages");
    }
  }
}


void KvazzupController::iceFailed(uint32_t sessionID)
{
  Logger::getLogger()->printError(this, "ICE has failed");

  userInterface_.showICEFailedMessage();

  // TODO: Tell sip manager to send an error for ICE
  Logger::getLogger()->printUnimplemented(this, "Send SIP error code for ICE failure");
  sessionTerminated(sessionID);
}


void KvazzupController::zrtpFailed(quint32 sessionID)
{
  Logger::getLogger()->printError(this, "ZRTP has failed");

  userInterface_.showZRTPFailedMessage(QString::number(sessionID));
  if (states_.find(sessionID) != states_.end() &&
      states_[sessionID].state != CALL_ENDING)
  {
    // TODO: Mutex this
    states_[sessionID].state = CALL_ENDING;
    sip_.endCall(sessionID);
    sessionTerminated(sessionID); // TODO: Remove this once the BYE OK is processed correctly in SIPManager
  }
}


void KvazzupController::noEncryptionAvailable()
{
  userInterface_.showCryptoMissingMessage();
}


void KvazzupController::createCall(uint32_t sessionID)
{
  std::shared_ptr<SDPMessageInfo> localSDP = states_[sessionID].localSDP;
  std::shared_ptr<SDPMessageInfo> remoteSDP = states_[sessionID].remoteSDP;

  if (localSDP == nullptr || remoteSDP == nullptr)
  {
    Logger::getLogger()->printError(this, "Failed to get SDP. Error should be detected earlier.");
    return;
  }

  bool videoEnabled = false;
  bool audioEnabled = false;

  if (states_[sessionID].followOurSDP)
  {
    for (auto& media : localSDP->media)
    {
      if (media.type == "audio")
      {
        audioEnabled = getReceiveAttribute(media, true);
      }
      else if (media.type == "video")
      {
        videoEnabled = getReceiveAttribute(media, true);
      }
    }
  }
  else
  {
    for (auto& media : remoteSDP->media)
    {
      if (media.type == "audio")
      {
        audioEnabled = getReceiveAttribute(media, false);
      }
      else if (media.type == "video")
      {
        videoEnabled = getReceiveAttribute(media, false);
      }
    }
  }

  QString videoState = "no";
  QString audioState = "no";

  if (videoEnabled)
  {
    videoState = "yes";
  }

  if (audioEnabled)
  {
    audioState = "yes";
  }

  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Creating call media",
                                  {"Video Enabled", "Audio Enabled"},
                                  {videoState, audioState});

  viewFactory_->getVideo(sessionID)->drawMicOffIcon(!audioEnabled);

  userInterface_.callStarted(sessionID, videoEnabled, audioEnabled,
                             viewFactory_->getView(sessionID), states_[sessionID].name);

  if (!states_[sessionID].sessionRunning)
  {
    if (stats_)
    {
      stats_->addSession(sessionID);
    }

    media_.addParticipant(sessionID, remoteSDP, localSDP, viewFactory_->getVideo(sessionID),
                           states_[sessionID].followOurSDP);

    states_[sessionID].sessionRunning = true;
   }
  else
  {
    media_.modifyParticipant(sessionID, remoteSDP, localSDP, viewFactory_->getVideo(sessionID),
                             states_[sessionID].followOurSDP);
  }

  // lastly we delete our saved SDP messages when they are no longer needed
  states_[sessionID].localSDP = nullptr;
  states_[sessionID].remoteSDP = nullptr;
}


void KvazzupController::cancelIncomingCall(uint32_t sessionID)
{
  removeSession(sessionID, "Cancelled", true);
}


void KvazzupController::sessionTerminated(uint32_t sessionID)
{
  Logger::getLogger()->printNormal(this, "Ending the call", {"SessionID"}, 
                                   {QString::number(sessionID)});
  if (states_.find(sessionID) != states_.end() &&
      (states_[sessionID].state == CALL_TRANSACTION_CONCLUDED ||
       states_[sessionID].state == CALL_ENDING))
  {
    media_.removeParticipant(sessionID);
  }
  removeSession(sessionID, "Call ended", true);
}


void KvazzupController::registeredToServer()
{
  Logger::getLogger()->printImportant(this, "We have been registered to a SIP server.");
  // TODO: indicate to user in some small detail
}


void KvazzupController::registeringFailed()
{
  Logger::getLogger()->printError(this, "Failed to register to a SIP server.");
  // TODO: indicate error to user
}


void KvazzupController::userAcceptsCall(uint32_t sessionID)
{
  Logger::getLogger()->printNormal(this, "We accept");
  states_[sessionID].state = CALL_TRANSACTION_CONCLUDED;
  sip_.respondOkToINVITE(sessionID);
}


void KvazzupController::userRejectsCall(uint32_t sessionID)
{
  Logger::getLogger()->printNormal(this, "We reject");
  sip_.respondDeclineToINVITE(sessionID);
  removeSession(sessionID, "Rejected", true);
}


void KvazzupController::userCancelsCall(uint32_t sessionID)
{
  Logger::getLogger()->printNormal(this, "We cancel our call");
  if (!sip_.cancelCall(sessionID))
  {
    userInterface_.removeParticipant(sessionID);
  }
}


void KvazzupController::endTheCall()
{
  Logger::getLogger()->printImportant(this, "We end the call");

  sip_.endAllCalls();
}


void KvazzupController::removeSession(uint32_t sessionID, QString message,
                                      bool temporaryMessage)
{
  if (message == "" || message.isEmpty())
  {
    userInterface_.removeParticipant(sessionID);
  }
  else
  {
    userInterface_.removeWithMessage(sessionID, message, temporaryMessage);
  }

  auto it = states_.find(sessionID);
  if(it != states_.end())
  {
    states_.erase(it);
  }

  if (stats_)
  {
    stats_->removeSession(sessionID);
  }

  viewFactory_->clearWidgets(sessionID);
}


void KvazzupController::SIPRequestCallback(uint32_t sessionID,
                                           SIPRequest& request,
                                           QVariant& content)
{
  Q_UNUSED(content);

  Logger::getLogger()->printNormal(this, "Got request callback",
                                   "type", QString::number(request.method));

  switch (request.method)
  {
    case SIP_INVITE:
    {
      if (states_.find(sessionID) != states_.end() &&
          (states_.at(sessionID).sessionNegotiated))
      {
        Logger::getLogger()->printNormal(this, "Detected a re-INVITE");
        ++ongoingNegotiations_;
        states_[sessionID].state = CALL_OK_SENT;
        sip_.respondOkToINVITE(sessionID);
      }
      else if (!processINVITE(sessionID, request.message->from.address.realname))
      {
        sip_.respondRingingToINVITE(sessionID);
      }
      else
      {
        states_[sessionID].state = CALL_OK_SENT;
        sip_.respondOkToINVITE(sessionID);
      }

      // the SDP may be either in INVITE or ACK
      getRemoteSDP(sessionID, request.message, content);

      break;
    }
    case SIP_ACK:
    {
    // the SDP may be either in INVITE or ACK
      getRemoteSDP(sessionID, request.message, content);
      INVITETransactionConcluded(sessionID);

      break;
    }
    case SIP_BYE:
    {
      sessionTerminated(sessionID);
      break;
    }
    case SIP_CANCEL:
    {
      cancelIncomingCall(sessionID);
      break;
    }
    default:
    {
      break;
    }
  }
}


void KvazzupController::SIPResponseCallback(uint32_t sessionID,
                                            SIPResponse& response,
                                            QVariant& content)
{
  Q_UNUSED(content);

  Logger::getLogger()->printNormal(this, "Got response callback",
                                   "Code", QString::number(response.type));

  if (response.type >= 100 && response.type <= 299)
  {
    if(response.message->cSeq.method == SIP_INVITE)
    {
      getRemoteSDP(sessionID, response.message, content);

      if(response.type == SIP_RINGING)
      {
        processRINGING(sessionID);
      }
      else if(response.type == SIP_OK)
      {
        processINVITE_OK(sessionID);
      }
    }
    else if (response.message->cSeq.method == SIP_BYE && response.type == 200)
    {
      sessionTerminated(sessionID);
    }
  }
  else if (response.type >= 300 && response.type <= 399)
  {
  }
  else if (response.type >= 400 && response.type <= 499)
  {
    if (response.type == SIP_REQUEST_TERMINATED)
    {
      if (response.message->cSeq.method == SIP_INVITE)
      {
        removeSession(sessionID, "Call cancelled", true);
      }
    }
    else if(response.type == SIP_NOT_FOUND)
    {
      removeSession(sessionID, "Not found", true);
    }

    // TODO: A bit of a hack, we should redo authentication INVITE here instead of elsewhere
    if (response.message->cSeq.method == SIP_INVITE &&
        response.type != SIP_PROXY_AUTHENTICATION_REQUIRED)
    {
      --ongoingNegotiations_;
    }

    // TODO: Put rest of the error return values
  }
  else if (response.type >= 500 && response.type <= 599)
  {

  }
  else if (response.type >= 600 && response.type <= 699)
  {
    if (response.message->cSeq.method == SIP_INVITE)
    {
      if (response.type == SIP_DECLINE)
      {
        sessionTerminated(sessionID);
      }
    }
  }
}


void KvazzupController::inputLocalSDP(uint32_t sessionID, std::shared_ptr<SDPMessageInfo> local)
{
  if (states_.find(sessionID) == states_.end())
  {
    Logger::getLogger()->printProgramError(this, "Got local SDP, but we are not in correct state");
    return;
  }

  states_[sessionID].localSDP = local;
  states_[sessionID].followOurSDP = true;

  if (states_[sessionID].remoteSDP != nullptr &&
      states_[sessionID].state == CALL_TRANSACTION_CONCLUDED)
  {
    INVITETransactionConcluded(sessionID);
  }
}


void KvazzupController::processRegisterRequest(QString address,
                                               SIPRequest& request,
                                               QVariant& content)
{
  Q_UNUSED(content);
  Logger::getLogger()->printNormal(this, "Got a register request callback",
                                   "type", QString::number(request.method));
}


void KvazzupController::processRegisterResponse(QString address,
                                                SIPResponse& response,
                                                QVariant& content)
{
  Q_UNUSED(content);
  Logger::getLogger()->printNormal(this, "Got a register response callback",
                                   "Code", QString::number(response.type));

  if (response.message->cSeq.method == SIP_REGISTER &&
      response.type == 200 &&
      !response.message->contact.isEmpty())
  {
    userInterface_.updateServerStatus("Registered");
  }
}


void KvazzupController::getRemoteSDP(uint32_t sessionID,
                                     std::shared_ptr<SIPMessageHeader> message,
                                     QVariant& content)
{
  if (states_.find(sessionID) == states_.end())
  {
    Logger::getLogger()->printError(this, "No call state when getting their SDP");
    return;
  }

  if (message->contentType == MT_APPLICATION_SDP)
  {
    SDPMessageInfo sdp = content.value<SDPMessageInfo>();
    states_[sessionID].remoteSDP = std::shared_ptr<SDPMessageInfo>(new SDPMessageInfo);
    *(states_[sessionID].remoteSDP) = sdp;

    states_[sessionID].followOurSDP = states_[sessionID].localSDP == nullptr;
  }
}


void KvazzupController::renegotiateCall(uint32_t sessionID)
{
  bool found = false;

  // avoid renegotiating the same call twice
  for (auto& pendingSessionID: pendingRenegotiations_)
  {
    if (pendingSessionID == sessionID)
    {
      found = true;
    }
  }

  if (!found)
  {
    pendingRenegotiations_.push_back(sessionID);
  }
}


void KvazzupController::renegotiateAllCalls()
{
  for (auto& state : states_)
  {
    renegotiateCall(state.first);
  }
}


void KvazzupController::negotiateNextCall()
{
  if (!pendingRenegotiations_.empty() && ongoingNegotiations_ == 0)
  {
    ++ongoingNegotiations_;
    uint32_t sessionID = pendingRenegotiations_.front();
    pendingRenegotiations_.pop_front();
    states_[sessionID].localSDP = nullptr;
    states_[sessionID].remoteSDP = nullptr;

    states_[sessionID].state = CALL_INVITE_SENT;
    sip_.sendINVITE(sessionID);
  }
}


void KvazzupController::checkBinding()
{
  int autoConnect = settingValue(SettingsKey::sipAutoConnect);
  if(autoConnect == 1)
  {
    QString serverAddress = settingString(SettingsKey::sipServerAddress);
    waitingToBind_.push_back(serverAddress);

    if(sip_.connect(SIP_TCP, serverAddress, 5060))
    {
      waitingToBind_.removeAll(serverAddress);
      sip_.bindingAtRegistrar(serverAddress);
    }
  }
}


void KvazzupController::connectionEstablished(QString localAddress, QString remoteAddress)
{
  Q_UNUSED(localAddress)

  // if we are planning to call a peer using this connection
  if (waitingToStart_.find(remoteAddress) != waitingToStart_.end())
  {
    WaitingStart startingSession = waitingToStart_[remoteAddress];
    waitingToStart_.erase(remoteAddress);

    createSIPDialog(startingSession.realname, startingSession.realname,
                    remoteAddress,
                    startingSession.sessionID);
  }

  // if we are planning to register using this connection
  if(waitingToBind_.contains(remoteAddress))
  {
    sip_.bindingAtRegistrar(remoteAddress);
    waitingToBind_.removeOne(remoteAddress);
  }
}
