#include "kvazzupcontroller.h"

#include "statisticsinterface.h"

#include "common.h"
#include "settingskeys.h"
#include "logger.h"

#include <QSettings>
#include <QHostAddress>

#include <thread>
#include <chrono>


KvazzupController::KvazzupController():
  states_(),
  media_(),
  sip_(),
  userInterface_(),
  stats_(nullptr),
  delayAutoAccept_(),
  delayedAutoAccept_(0)
{}


void KvazzupController::init()
{
  Logger::getLogger()->printImportant(this, "Kvazzup initiation Started");

  userInterface_.init(this);

  stats_ = userInterface_.createStatsWindow();

  // connect sip signals so we get information when ice is ready
  QObject::connect(&sip_, &SIPManager::nominationSucceeded,
                   this, &KvazzupController::iceCompleted);

  QObject::connect(&sip_, &SIPManager::nominationFailed,
                   this, &KvazzupController::iceFailed);

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

  QObject::connect(&media_, &MediaManager::handleZRTPFailure,
                   this,    &KvazzupController::zrtpFailed);

  QObject::connect(&media_, &MediaManager::handleNoEncryption,
                   this,    &KvazzupController::noEncryptionAvailable);


  media_.init(userInterface_.getViewFactory(), stats_);

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


uint32_t KvazzupController::callToParticipant(QString name, QString username,
                                              QString ip)
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

  uint32_t sessionID = sip_.startCall(remote);

  userInterface_.displayOutgoingCall(sessionID, remote.realname);
  if(states_.find(sessionID) == states_.end())
  {
    states_[sessionID] = SessionState{CALLINGTHEM , nullptr, nullptr};
  }

  return sessionID;
}


uint32_t KvazzupController::chatWithParticipant(QString name, QString username,
                                                QString ip)
{
  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Starting a chat with contact",
            {"ip", "Name", "Username"}, {ip, name, username});
  return 0;
}


bool KvazzupController::incomingCall(uint32_t sessionID, QString caller)
{
  if(states_.find(sessionID) != states_.end())
  {
    Logger::getLogger()->printProgramError(this, "Incoming call is overwriting "
                                                 "an existing session!");
  }

  int autoAccept = settingValue(SettingsKey::localAutoAccept);

  if(autoAccept == 1)
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
    states_[sessionID] = SessionState{CALLRINGINGWITHUS, nullptr, nullptr};
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

  updateSDPAudioStatus(sdp);

  sip_.setSDP(sdp);
  emit media_.updateAudioSettings();
}


void KvazzupController::updateVideoSettings()
{
  std::shared_ptr<SDPMessageInfo> sdp = sip_.generateSDP(getLocalUsername(), 1, 1,
                                                         {"opus"}, {"H265"}, {0}, {});

  updateSDPVideoStatus(sdp);

  sip_.setSDP(sdp);
  emit media_.updateVideoSettings();
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


void KvazzupController::callRinging(uint32_t sessionID)
{
  // TODO_RFC 3261: Enable cancelling the request at this point
  // to make sure the original request has been received

  if(states_.find(sessionID) != states_.end() && states_[sessionID].state == CALLINGTHEM)
  {
    Logger::getLogger()->printNormal(this, "Our call is ringing");
    userInterface_.displayRinging(sessionID);
    states_[sessionID].state = CALLRINGINWITHTHEM;
  }
  else
  {
    Logger::getLogger()->printPeerError(this, "Got call ringing for nonexisting call",
                  {"SessionID"}, {QString::number(sessionID)});
  }
}

void KvazzupController::peerAccepted(uint32_t sessionID)
{
  if(states_.find(sessionID) != states_.end())
  {
    if(states_[sessionID].state == CALLRINGINWITHTHEM || states_[sessionID].state == CALLINGTHEM)
    {
      Logger::getLogger()->printImportant(this, "They accepted our call!");
      states_[sessionID].state = CALLNEGOTIATING;
    }
    else
    {
      Logger::getLogger()->printPeerError(this, "Got an accepted call even though "
                                                "we have not yet called them!");
    }
  }
  else
  {
    Logger::getLogger()->printPeerError(this, "Peer accepted a session which is not in Controller.");
  }
}


void KvazzupController::iceCompleted(quint32 sessionID,
                                     const std::shared_ptr<SDPMessageInfo> local,
                                     const std::shared_ptr<SDPMessageInfo> remote)
{
  Logger::getLogger()->printNormal(this, "ICE has been successfully completed",
            {"SessionID"}, {QString::number(sessionID)});

  if (states_.find(sessionID) != states_.end())
  {
    states_[sessionID].localSDP = local;
    states_[sessionID].remoteSDP = remote;

    if (states_[sessionID].state == CALLWAITINGICE)
    {
      createSingleCall(sessionID);
    }
  }
  else
  {
    Logger::getLogger()->printError(this, "Got a ICE completion on non-existing call!");
  }
}


void KvazzupController::callNegotiated(uint32_t sessionID)
{
  Logger::getLogger()->printNormal(this, "Call negotiated");

  if (states_.find(sessionID) != states_.end() &&
      states_[sessionID].state == CALLNEGOTIATING)
  {
    if (states_[sessionID].localSDP != nullptr &&
        states_[sessionID].remoteSDP != nullptr)
    {
      createSingleCall(sessionID);
    }
    else
    {
      states_[sessionID].state = CALLWAITINGICE;
    }
  }
}


void KvazzupController::iceFailed(uint32_t sessionID)
{
  Logger::getLogger()->printError(this, "ICE has failed");

  userInterface_.showICEFailedMessage();

  // TODO: Tell sip manager to send an error for ICE
  Logger::getLogger()->printUnimplemented(this, "Send SIP error code for ICE failure");
  endCall(sessionID);
}

void KvazzupController::zrtpFailed(quint32 sessionID)
{
  Logger::getLogger()->printError(this, "ZRTP has failed");

  userInterface_.showZRTPFailedMessage(QString::number(sessionID));
  endCall(sessionID);
}


void KvazzupController::noEncryptionAvailable()
{
  userInterface_.showCryptoMissingMessage();
}


void KvazzupController::createSingleCall(uint32_t sessionID)
{
  Logger::getLogger()->printNormal(this, "Call has been agreed upon with peer.",
              "SessionID", {QString::number(sessionID)});

  if (states_[sessionID].state == CALLONGOING)
  {
    // we have to remove previous media so we do not double them.
    media_.removeParticipant(sessionID);
    userInterface_.removeParticipant(sessionID);
  }

  std::shared_ptr<SDPMessageInfo> localSDP = states_[sessionID].localSDP;
  std::shared_ptr<SDPMessageInfo> remoteSDP = states_[sessionID].remoteSDP;

  bool videoEnabled = false;
  bool audioEnabled = false;

  for (auto& media : localSDP->media)
  {
    if (media.type == "video" && !videoEnabled)
    {
      videoEnabled = (media.flagAttributes.empty()
                      || media.flagAttributes.at(0) == A_NO_ATTRIBUTE
                      || media.flagAttributes.at(0) == A_SENDRECV
                      || media.flagAttributes.at(0) == A_RECVONLY);
    }
    else if (media.type == "audio" && !audioEnabled)
    {
      audioEnabled = (media.flagAttributes.empty()
                      || media.flagAttributes.at(0) == A_NO_ATTRIBUTE
                      || media.flagAttributes.at(0) == A_SENDRECV
                      || media.flagAttributes.at(0) == A_RECVONLY);
    }
  }

  userInterface_.callStarted(sessionID, videoEnabled, audioEnabled);

  if(localSDP == nullptr || remoteSDP == nullptr)
  {
    Logger::getLogger()->printError(this, "Failed to get SDP. Error should be detected earlier.");
    return;
  }

  if (stats_)
  {
    stats_->addSession(sessionID);
  }

  media_.addParticipant(sessionID, remoteSDP, localSDP);
  states_[sessionID].state = CALLONGOING;
}


void KvazzupController::cancelIncomingCall(uint32_t sessionID)
{
  removeSession(sessionID, "Cancelled", true);
}


void KvazzupController::endCall(uint32_t sessionID)
{
  Logger::getLogger()->printNormal(this, "Ending the call", {"SessionID"}, 
                                   {QString::number(sessionID)});
  if (states_.find(sessionID) != states_.end() &&
      states_[sessionID].state == CALLONGOING)
  {
    media_.removeParticipant(sessionID);
  }
  removeSession(sessionID, "Call ended", true);
}


void KvazzupController::failure(uint32_t sessionID, QString error)
{
  if (states_.find(sessionID) != states_.end())
  {
    if (states_[sessionID].state == CALLINGTHEM)
    {
      Logger::getLogger()->printImportant(this, "Our call failed. Invalid sip address?");
    }
    else if(states_[sessionID].state == CALLRINGINWITHTHEM)
    {
      Logger::getLogger()->printImportant(this, "Our call has been rejected!");
    }
    else
    {
      Logger::getLogger()->printPeerError(this, "Got reject when we weren't calling them", 
                                          "SessionID", {QString::number(sessionID)});
    }
    removeSession(sessionID, error, false);
  }
  else
  {
    Logger::getLogger()->printPeerError(this, "Got reject for nonexisting call", 
                                        "SessionID", {QString::number(sessionID)});
    Logger::getLogger()->printPeerError(this, "", "Ongoing sessions", 
                                        {QString::number(states_.size())});
  }
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
  states_[sessionID].state = CALLNEGOTIATING;
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
  sip_.cancelCall(sessionID);
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
}

void KvazzupController::SIPRequestCallback(uint32_t sessionID,
                                           SIPRequest& request,
                                           QVariant& content)
{
  Logger::getLogger()->printNormal(this, "Got request callback",
                                   "type", QString::number(request.method));

  switch (request.method)
  {
    case SIP_INVITE:
    {
      if (!incomingCall(sessionID, request.message->from.address.realname))
      {
        sip_.respondRingingToINVITE(sessionID);
      }
      else
      {
        sip_.respondOkToINVITE(sessionID);
      }

      break;
    }
    case SIP_ACK:
    {
      callNegotiated(sessionID);
      break;
    }
    case SIP_BYE:
    {
      endCall(sessionID);
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
  Logger::getLogger()->printNormal(this, "Got response callback",
                                   "Code", QString::number(response.type));

  if (response.type >= 100 && response.type <= 299)
  {
    if(response.message->cSeq.method == SIP_INVITE)
    {
      if(response.type == SIP_RINGING)
      {
        callRinging(sessionID);
      }
      else if(response.type == SIP_OK)
      {
        peerAccepted(sessionID);
        callNegotiated(sessionID);
      }
    }
    else if (response.message->cSeq.method == SIP_BYE && response.type == 200)
    {
      endCall(sessionID);
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
        endCall(sessionID);
      }
    }
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
