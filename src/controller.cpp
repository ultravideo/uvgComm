#include "controller.h"

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


uvgCommController::uvgCommController():
  states_(),
  media_(),
  sip_(),
  userInterface_(),
  stats_(nullptr),
  delayAutoAccept_(),
  delayedAutoAccept_(0),
  viewFactory_(std::shared_ptr<VideoviewFactory>(new VideoviewFactory())),
  ongoingNegotiations_(0),
  delayedNegotiation_()
{}


void uvgCommController::init()
{
  Logger::getLogger()->printImportant(this, "uvgComm initiation Started");

  userInterface_.init(this, viewFactory_);

  stats_ = userInterface_.createStatsWindow();

  QObject::connect(&sip_, &SIPManager::finalLocalSDP,
                   this, &uvgCommController::inputLocalSDP);

  QObject::connect(&sip_, &SIPManager::connectionEstablished,
                   this, &uvgCommController::connectionEstablished);


  sip_.installSIPRequestCallback(std::bind(&uvgCommController::SIPRequestCallback, this,
                                           std::placeholders::_1,
                                           std::placeholders::_2,
                                           std::placeholders::_3));
  sip_.installSIPResponseCallback(std::bind(&uvgCommController::SIPResponseCallback, this,
                                            std::placeholders::_1,
                                            std::placeholders::_2,
                                            std::placeholders::_3));

  sip_.installSIPRequestCallback(std::bind(&uvgCommController::processRegisterRequest, this,
                                           std::placeholders::_1,
                                           std::placeholders::_2,
                                           std::placeholders::_3));
  sip_.installSIPResponseCallback(std::bind(&uvgCommController::processRegisterResponse, this,
                                            std::placeholders::_1,
                                            std::placeholders::_2,
                                            std::placeholders::_3));

  std::shared_ptr<SDPMessageInfo> sdp = sip_.generateSDP(getLocalUsername(), 1, 1,
                                                         {"opus"}, {"H265"}, {0}, {});

  updateSDPAudioStatus(sdp);
  updateSDPVideoStatus(sdp);

  sip_.setSDP(sdp);

  sip_.init(createSIPConfig(), stats_);
  sip_.listenToAny(SIP_TCP, 5060);

  updateCallSettings();

  QObject::connect(&media_, &MediaManager::handleZRTPFailure,
                   this,    &uvgCommController::zrtpFailed);

  QObject::connect(&media_, &MediaManager::handleNoEncryption,
                   this,    &uvgCommController::noEncryptionAvailable);

  // connect sip signal so we get information when ice fails
  QObject::connect(&media_, &MediaManager::iceMediaFailed,
                   this, &uvgCommController::iceFailed);

  media_.init(viewFactory_, stats_);

  // register the GUI signals indicating GUI changes to be handled
  // approrietly in a system wide manner
  QObject::connect(&userInterface_, &UIManager::updateCallSettings,
                   this, &uvgCommController::updateCallSettings);

  QObject::connect(&userInterface_, &UIManager::updateVideoSettings,
                   this, &uvgCommController::updateVideoSettings);
  QObject::connect(&userInterface_, &UIManager::updateAudioSettings,
                   this, &uvgCommController::updateAudioSettings);

  QObject::connect(&userInterface_, &UIManager::updateAutomaticSettings,
                   &media_, &MediaManager::updateAutomaticSettings);

  QObject::connect(&userInterface_, &UIManager::endCall, this, &uvgCommController::endTheCall);
  QObject::connect(&userInterface_, &UIManager::quit, this, &uvgCommController::quit);

  QObject::connect(&userInterface_, &UIManager::callAccepted,
                   this, &uvgCommController::userAcceptsCall);
  QObject::connect(&userInterface_, &UIManager::callRejected,
                   this, &uvgCommController::userRejectsCall);
  QObject::connect(&userInterface_, &UIManager::callCancelled,
                   this, &uvgCommController::userCancelsCall);

  // lastly, show the window when our signals are ready
  userInterface_.showMainWindow();

  Logger::getLogger()->printImportant(this, "uvgComm initiation finished");
}


void uvgCommController::uninit()
{
  // for politeness, we send BYE messages to all our call participants.
  endTheCall();
  sip_.uninit();
  media_.uninit();
}


SIPConfig uvgCommController::createSIPConfig()
{
  return {settingEnabled(SettingsKey::sipAutoConnect),
          settingString(SettingsKey::sipServerAddress),
          5060,
          settingEnabled(SettingsKey::sipP2PConferencing),
          (uint16_t)settingValue(SettingsKey::sipMediaPort),
#ifdef uvgComm_NO_RTP_MULTIPLEXING
          settingEnabled(SettingsKey::sipICEEnabled),
#else
        false,
#endif
          settingEnabled(SettingsKey::privateAddresses),
          settingEnabled(SettingsKey::sipSTUNEnabled),
          settingString(SettingsKey::sipSTUNAddress),
          (uint16_t)settingValue(SettingsKey::sipSTUNPort)};
}


void uvgCommController::quit()
{
  uninit();
}


uint32_t uvgCommController::startINVITETransaction(QString name, QString username,
                                                   QString ip)
{
  uint32_t sessionID = sip_.reserveSessionID();

  // IPv6 address can be concatanated, so here we uniformalize their representation
  QHostAddress qIP = QHostAddress(ip);

  waitingToStart_[qIP.toString()] = {name, username, sessionID};

  // try connecting, if returns immediately or we already have a connection, create Dialog
  if (sip_.connect(SIP_TCP, ip, 5060))
  {
    waitingToStart_.erase(ip);
    createSIPDialog(name, username, ip, sessionID);
  }

  return sessionID;
}


void uvgCommController::createSIPDialog(QString name, QString username, QString ip, uint32_t sessionID)
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
    states_[sessionID] = SessionState{CALL_INVITE_SENT, nullptr, nullptr, false, true, false, false, name};
  }
  else
  {
    Logger::getLogger()->printProgramError(this, "SIP Manager is giving existing sessionIDs as new ones");
  }

  // first we must renegotiates this call, so we get all their media parameters
  renegotiateCall(sessionID);

  if (settingEnabled(SettingsKey::sipP2PConferencing))
  {
    // then we renegotiates rest of the calls with the previous calls parameters included
    for (auto& state : states_)
    {
      if (state.first != sessionID)
      {
        renegotiateCall(state.first);
      }
    }
  }

  if (ongoingNegotiations_ == 0)
  {
    negotiateNextCall();
  }
}


uint32_t uvgCommController::chatWithParticipant(QString name, QString username,
                                                QString ip)
{
  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Starting a chat with contact",
            {"ip", "Name", "Username"}, {ip, name, username});
  return 0;
}


bool uvgCommController::processINVITE(uint32_t sessionID, QString caller)
{
  if(states_.find(sessionID) != states_.end())
  {
    Logger::getLogger()->printProgramError(this, "Incoming call is overwriting "
                                                 "an existing session!");
  }

  states_[sessionID] = SessionState{CALL_INVITE_RECEIVED, nullptr, nullptr, true, false, false, false, caller};
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
    delayAutoAccept_.singleShot(100, this, &uvgCommController::delayedAutoAccept);
    return false;
  }
  else
  {
    Logger::getLogger()->printNormal(this, "Showing incoming call");
    userInterface_.displayIncomingCall(sessionID, caller);
  }
  return false;
}


void uvgCommController::delayedAutoAccept()
{
  userAcceptsCall(delayedAutoAccept_);
  delayedAutoAccept_ = 0;
}


void uvgCommController::updateAudioSettings()
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


void uvgCommController::updateVideoSettings()
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


void uvgCommController::updateCallSettings()
{
  SIPConfig config = createSIPConfig();

  bool autoConnect = config.sendRegister;
  if(autoConnect)
  {
    QString serverAddress = config.sipServerAddress;
    waitingToBind_.push_back(serverAddress);

    if(sip_.connect(SIP_TCP, serverAddress, config.sipServerPort))
    {
      waitingToBind_.removeAll(serverAddress);
      sip_.bindingAtRegistrar(serverAddress);
    }
  }

  sip_.setConfig(config);
}


void uvgCommController::updateSDPAudioStatus(std::shared_ptr<SDPMessageInfo> sdp)
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


void uvgCommController::updateSDPVideoStatus(std::shared_ptr<SDPMessageInfo> sdp)
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


void uvgCommController::processRINGING(uint32_t sessionID)
{
  // TODO_RFC 3261: Enable cancelling the request at this point
  // to make sure the original request has been received

  if(states_.find(sessionID) != states_.end() &&
     states_[sessionID].state == CALL_INVITE_SENT &&
     !states_[sessionID].sessionRunning)
  {
    Logger::getLogger()->printNormal(this, "Our call is ringing");
    userInterface_.displayRinging(sessionID);
  }
  else
  {
    Logger::getLogger()->printPeerError(this, "Got invalid ringing reply",
                  {"SessionID"}, {QString::number(sessionID)});
  }
}


void uvgCommController::processINVITE_OK(uint32_t sessionID)
{
  if(states_.find(sessionID) != states_.end())
  {
    if(states_[sessionID].state == CALL_INVITE_SENT)
    {
      Logger::getLogger()->printImportant(this, "They accepted our call!");
      states_[sessionID].followOurSDP = false;
      states_[sessionID].iceController = areWeICEController(true, sessionID);
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


void uvgCommController::INVITETransactionConcluded(uint32_t sessionID)
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
                                       this, &uvgCommController::negotiateNextCall);
      }
    }
    else
    {
      Logger::getLogger()->printNormal(this, "Still waiting for SDP messages");
    }
  }
}


void uvgCommController::iceFailed(uint32_t sessionID)
{
  Logger::getLogger()->printError(this, "ICE has failed");

  userInterface_.showICEFailedMessage();

  // TODO: Tell sip manager to send an error for ICE
  Logger::getLogger()->printUnimplemented(this, "Send SIP error code for ICE failure");
  sessionTerminated(sessionID);
}


void uvgCommController::zrtpFailed(quint32 sessionID)
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


void uvgCommController::noEncryptionAvailable()
{
  userInterface_.showCryptoMissingMessage();
}


void uvgCommController::createCall(uint32_t sessionID)
{
  std::shared_ptr<SDPMessageInfo> localSDP = states_[sessionID].localSDP;
  std::shared_ptr<SDPMessageInfo> remoteSDP = states_[sessionID].remoteSDP;

  if (localSDP == nullptr || remoteSDP == nullptr)
  {
    Logger::getLogger()->printError(this, "Failed to get SDP. Error should be detected earlier.");
    return;
  }

  if (localSDP->media.size() != remoteSDP->media.size())
  {
    Logger::getLogger()->printError(this, "The SDPs have different amount of medias");
    return;
  }

  for(auto& media : localSDP->media)
  {
    getStunBindings(sessionID, media);
  }

  // Here we remove media which is not ours. This happens in mesh conference, when we are the host
  std::vector<unsigned int> toDelete;
  for (unsigned int index = 0; index < localSDP->media.size(); ++index)
  {
    if (!isLocalAddress(localSDP->media.at(index).connection_address))
    {
      toDelete.push_back(index);
    }
  }

  for (unsigned int j = toDelete.size(); j > 0; --j)
  {
    unsigned int index = toDelete.at(j - 1);

    localSDP->media.erase(localSDP->media.begin() + index);
    remoteSDP->media.erase(remoteSDP->media.begin() + index);
  }

  QList<std::pair<MediaID, MediaID>> audioVideoIDs;
  QList<MediaID> allIDs;

  updateMediaIDs(sessionID, localSDP->media, remoteSDP->media,
                 states_[sessionID].followOurSDP, audioVideoIDs, allIDs);

  QStringList names;

  // TODO: We should implement some way to have access
  // to actual name of participant in conference call
  for (auto& media : states_[sessionID].localSDP->media)
  {
    names.push_back(states_[sessionID].name);
  }

  userInterface_.callStarted(viewFactory_, sessionID, names, audioVideoIDs);

  if (!states_[sessionID].sessionRunning)
  {
    if (stats_)
    {
      stats_->addSession(sessionID);
    }

    media_.addParticipant(sessionID, remoteSDP, localSDP, allIDs,
                          states_[sessionID].iceController,
                          states_[sessionID].followOurSDP);

    states_[sessionID].sessionRunning = true;
   }
  else
  {
    media_.modifyParticipant(sessionID, remoteSDP, localSDP, allIDs,
                             states_[sessionID].iceController,
                             states_[sessionID].followOurSDP);
  }

  // lastly we delete our saved SDP messages when they are no longer needed
  states_[sessionID].localSDP = nullptr;
  states_[sessionID].remoteSDP = nullptr;
}


void uvgCommController::updateMediaIDs(uint32_t sessionID,
                                       QList<MediaInfo>& localMedia,
                                       QList<MediaInfo>& remoteMedia,
                                       bool followOurSDP,
                                       QList<std::pair<MediaID, MediaID>> &audioVideoIDs,
                                       QList<MediaID>& allIDs)
{
  Q_ASSERT(localMedia.size() == remoteMedia.size());

  QStringList medias = {"SessionID"};
  QStringList ssrcs = {QString::number(sessionID)};

  for (auto& media : localMedia)
  {
    medias.push_back(media.type + " ssrc");
    ssrcs.push_back(QString::number(findSSRC(media)));
  }

  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Getting mediaIDs for " +
                                                        QString::number(localMedia.size()) + " medias",
                                  {medias}, {ssrcs});

  // first we set correct attributes
  for (int i = 0; i < localMedia.size(); i += 1)
  {
    bool send = false;
    bool receive = false;

    if (!localMedia.at(i).candidates.empty())
    {
      if (isLocalCandidate(localMedia.at(i).candidates.first())) // if we are using ICE
      {
        getMediaAttributes(localMedia.at(i), remoteMedia.at(i), followOurSDP, send, receive);

        allIDs.push_back(getMediaID(sessionID, localMedia.at(i)));

        allIDs.back().setReceive(receive);
        allIDs.back().setSend(send);
      }
    }
    else if (isLocalAddress(localMedia.at(i).connection_address)) // if we are not using ICE
    {
      getMediaAttributes(localMedia.at(i), remoteMedia.at(i), followOurSDP, send, receive);

      allIDs.push_back(getMediaID(sessionID, localMedia.at(i)));

      allIDs.back().setReceive(receive);
      allIDs.back().setSend(send);
    }
    else
    {
      Logger::getLogger()->printNormal(this, "Remote candidate, not processing. Happens when we are the conference host",
                                       "Address", localMedia.at(i).connection_address);
    }
  }

  // TODO: Use lipsync to determine pairs
  for (int i = 0; i < allIDs.size(); i +=2)
  {
    audioVideoIDs.push_back({allIDs.at(i), allIDs.at(i + 1)});
  }
}


void uvgCommController::getMediaAttributes(const MediaInfo &local, const MediaInfo &remote,
                                           bool followOurSDP,
                                           bool& send, bool& receive)
{
  if (followOurSDP)
  {
    receive = getReceiveAttribute(local, followOurSDP);
    send =    getSendAttribute(local, followOurSDP);
  }
  else
  {
    receive = getReceiveAttribute(remote, followOurSDP);
    send =    getSendAttribute(remote, followOurSDP);
  }
}


MediaID uvgCommController::getMediaID(uint32_t sessionID, const MediaInfo &media)
{

  if (sessionMedias_.find(sessionID) != sessionMedias_.end())
  {
    // try to see if this is an old media
    for (auto& sMedia : *sessionMedias_[sessionID])
    {
      if (sMedia == media)
      {
        Logger::getLogger()->printNormal(this, "Using existing MediaID",
                                         "Media type", media.type);
        return sMedia;
      }
    }
  }
  else
  {
    sessionMedias_[sessionID] =
      std::shared_ptr<std::vector<MediaID>>(new std::vector<MediaID>());
  }

  Logger::getLogger()->printNormal(this, "Creating a new MediaID",
                                   "SSRC", QString::number(findSSRC(media)));

  // create a new ID for this media
  sessionMedias_[sessionID]->push_back(MediaID(media));
  return sessionMedias_[sessionID]->back();
}


void uvgCommController::cancelIncomingCall(uint32_t sessionID)
{
  removeSession(sessionID, "Cancelled", true);
}


void uvgCommController::sessionTerminated(uint32_t sessionID)
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


void uvgCommController::registeredToServer()
{
  Logger::getLogger()->printImportant(this, "We have been registered to a SIP server.");
  // TODO: indicate to user in some small detail
}


void uvgCommController::registeringFailed()
{
  Logger::getLogger()->printError(this, "Failed to register to a SIP server.");
  // TODO: indicate error to user
}


void uvgCommController::userAcceptsCall(uint32_t sessionID)
{
  Logger::getLogger()->printNormal(this, "We accept");
  states_[sessionID].state = CALL_TRANSACTION_CONCLUDED;
  states_[sessionID].followOurSDP = true;
  sip_.respondOkToINVITE(sessionID);
}


void uvgCommController::userRejectsCall(uint32_t sessionID)
{
  Logger::getLogger()->printNormal(this, "We reject");
  sip_.respondDeclineToINVITE(sessionID);
  removeSession(sessionID, "Rejected", true);

  --ongoingNegotiations_;
}


void uvgCommController::userCancelsCall(uint32_t sessionID)
{
  Logger::getLogger()->printNormal(this, "We cancel our call");
  if (!sip_.cancelCall(sessionID))
  {
    userInterface_.removeParticipant(sessionID);
  }

  --ongoingNegotiations_;
}


void uvgCommController::endTheCall()
{
  Logger::getLogger()->printImportant(this, "We end the call");

  sip_.endAllCalls();
}


void uvgCommController::removeSession(uint32_t sessionID, QString message,
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

  if(sessionMedias_.find(sessionID) != sessionMedias_.end())
  {
    for (auto& media : *sessionMedias_[sessionID])
    {
      viewFactory_->clearWidgets(media);
    }
  }

  if (stats_)
  {
    stats_->removeSession(sessionID);
  }
}


void uvgCommController::SIPRequestCallback(uint32_t sessionID,
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
      states_[sessionID].followOurSDP = true;
      states_[sessionID].iceController = areWeICEController(false, sessionID);
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


void uvgCommController::SIPResponseCallback(uint32_t sessionID,
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
        --ongoingNegotiations_;
      }
    }
  }
}


void uvgCommController::inputLocalSDP(uint32_t sessionID, std::shared_ptr<SDPMessageInfo> local)
{
  if (states_.find(sessionID) == states_.end())
  {
    Logger::getLogger()->printProgramError(this, "Got local SDP, but we are not in correct state");
    return;
  }

  states_[sessionID].localSDP = local;

  if (states_[sessionID].remoteSDP != nullptr &&
      states_[sessionID].state == CALL_TRANSACTION_CONCLUDED)
  {
    INVITETransactionConcluded(sessionID);
  }
}


void uvgCommController::processRegisterRequest(QString address,
                                               SIPRequest& request,
                                               QVariant& content)
{
  Q_UNUSED(content);
  Logger::getLogger()->printNormal(this, "Got a register request callback",
                                   "type", QString::number(request.method));
}


void uvgCommController::processRegisterResponse(QString address,
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


void uvgCommController::getRemoteSDP(uint32_t sessionID,
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
  }
}


void uvgCommController::renegotiateCall(uint32_t sessionID)
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


void uvgCommController::renegotiateAllCalls()
{
  for (auto& state : states_)
  {
    renegotiateCall(state.first);
  }
}


void uvgCommController::negotiateNextCall()
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


void uvgCommController::connectionEstablished(QString localAddress, QString remoteAddress)
{
  Q_UNUSED(localAddress)

  // if we are planning to call a peer using this connection
  if (waitingToStart_.find(remoteAddress) != waitingToStart_.end())
  {
    WaitingStart startingSession = waitingToStart_[remoteAddress];
    waitingToStart_.erase(remoteAddress);

    createSIPDialog(startingSession.realname, startingSession.username,
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


bool uvgCommController::areWeICEController(bool initialAgent, uint32_t sessionID) const
{
  // one to one calls should follow the specification
  if (states_.at(sessionID).remoteSDP &&
      states_.at(sessionID).remoteSDP->media.size() == 2)
  {
    Logger::getLogger()->printNormal(this, "One-to-one call so we follow specification");
    return initialAgent;
  }

  Logger::getLogger()->printNormal(this, "We select ICE controller based on a hack");

  return areWeFocus() || states_.at(sessionID).sessionRunning;
}


void uvgCommController::getStunBindings(uint32_t sessionID, MediaInfo& media)
{
  if (media.candidates.empty())
  {
    std::pair<QHostAddress, uint16_t> stunAddress(media.connection_address, media.receivePort);
    std::pair<QHostAddress, uint16_t> stunBinding;
    if (sip_.getSTUNBinding(sessionID, stunAddress, stunBinding))
    {
      media.connection_address = stunBinding.first.toString();
      media.receivePort = stunBinding.second;
    }
  }
  else
  {
    for(auto& candidate : media.candidates)
    {
      if (candidate->type == "srflx")
      {
        // this happens if we don't want to send our private addresses to peers
        if (candidate->rel_address == "" || candidate->rel_port == 0)
        {
          // these were sent to the other end
          std::pair<QHostAddress, uint16_t> stunAddress(candidate->address, candidate->port);
          std::pair<QHostAddress, uint16_t> stunBinding;

          if (sip_.getSTUNBinding(sessionID, stunAddress, stunBinding))
          {
            candidate->rel_address = stunBinding.first.toString();
            candidate->rel_port = stunBinding.second;
          }
          else
          {
            Logger::getLogger()->printError(this, "Could not get srflx address to bind to");
          }
        }
      }
    }
  }
}
