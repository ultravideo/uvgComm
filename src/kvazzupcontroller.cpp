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
  window_(nullptr),
  stats_(nullptr),
  delayAutoAccept_(),
  delayedAutoAccept_(0)
{}

void KvazzupController::init()
{
  Logger::getLogger()->printImportant(this, "Kvazzup initiation Started");

  window_.init(this);

  stats_ = window_.createStatsWindow();

  // connect sip signals so we get information when ice is ready
  QObject::connect(&sip_, &SIPManager::nominationSucceeded,
                   this, &KvazzupController::iceCompleted);
  QObject::connect(&sip_, &SIPManager::nominationFailed,
                   this, &KvazzupController::iceFailed);

  sip_.init(this, stats_, window_.getStatusView());

  QObject::connect(&media_, &MediaManager::handleZRTPFailure,
                   this,    &KvazzupController::zrtpFailed);

  QObject::connect(&media_, &MediaManager::handleNoEncryption,
                   this,    &KvazzupController::noEncryptionAvailable);


  media_.init(window_.getViewFactory(), stats_);

  // register the GUI signals indicating GUI changes to be handled
  // approrietly in a system wide manner
  QObject::connect(&window_, &CallWindow::updateCallSettings,
                   &sip_, &SIPManager::updateCallSettings);

  QObject::connect(&window_, &CallWindow::updateVideoSettings,
                   &media_, &MediaManager::updateVideoSettings);
  QObject::connect(&window_, &CallWindow::updateAudioSettings,
                   &media_, &MediaManager::updateAudioSettings);

  QObject::connect(&window_, SIGNAL(endCall()), this, SLOT(endTheCall()));
  QObject::connect(&window_, SIGNAL(closed()), this, SLOT(windowClosed()));

  QObject::connect(&window_, &CallWindow::callAccepted,
                   this, &KvazzupController::userAcceptsCall);
  QObject::connect(&window_, &CallWindow::callRejected,
                   this, &KvazzupController::userRejectsCall);
  QObject::connect(&window_, &CallWindow::callCancelled,
                   this, &KvazzupController::userCancelsCall);

  // lastly, show the window when our signals are ready
  window_.show();

  Logger::getLogger()->printImportant(this, "Kvazzup initiation finished");
}

void KvazzupController::uninit()
{
  // for politeness, we send BYE messages to all our call participants.
  endTheCall();
  sip_.uninit();
  media_.uninit();
}

void KvazzupController::windowClosed()
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

  return sip_.startCall(remote);
}

uint32_t KvazzupController::chatWithParticipant(QString name, QString username,
                                                QString ip)
{
  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Starting a chat with contact",
            {"ip", "Name", "Username"}, {ip, name, username});
  return 0;
}

void KvazzupController::outgoingCall(uint32_t sessionID, QString callee)
{
  window_.displayOutgoingCall(sessionID, callee);
  if(states_.find(sessionID) == states_.end())
  {
    states_[sessionID] = SessionState{CALLINGTHEM , nullptr, nullptr};
  }
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
    window_.displayIncomingCall(sessionID, caller);
  }
  return false;
}

void KvazzupController::delayedAutoAccept()
{
  userAcceptsCall(delayedAutoAccept_);
  delayedAutoAccept_ = 0;
}

void KvazzupController::callRinging(uint32_t sessionID)
{
  // TODO_RFC 3261: Enable cancelling the request at this point
  // to make sure the original request has been received

  if(states_.find(sessionID) != states_.end() && states_[sessionID].state == CALLINGTHEM)
  {
    Logger::getLogger()->printNormal(this, "Our call is ringing");
    window_.displayRinging(sessionID);
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

  window_.showICEFailedMessage();

  // TODO: Tell sip manager to send an error for ICE
  Logger::getLogger()->printUnimplemented(this, "Send SIP error code for ICE failure");
  endCall(sessionID);
}

void KvazzupController::zrtpFailed(quint32 sessionID)
{
  Logger::getLogger()->printError(this, "ZRTP has failed");

  window_.showZRTPFailedMessage(QString::number(sessionID));
  endCall(sessionID);
}


void KvazzupController::noEncryptionAvailable()
{
  window_.showCryptoMissingMessage();
}


void KvazzupController::createSingleCall(uint32_t sessionID)
{
  Logger::getLogger()->printNormal(this, "Call has been agreed upon with peer.",
              "SessionID", {QString::number(sessionID)});

  if (states_[sessionID].state == CALLONGOING)
  {
    // we have to remove previous media so we do not double them.
    media_.removeParticipant(sessionID);
    window_.removeParticipant(sessionID);
  }

  std::shared_ptr<SDPMessageInfo> localSDP = states_[sessionID].localSDP;
  std::shared_ptr<SDPMessageInfo> remoteSDP = states_[sessionID].remoteSDP;

  for (auto& media : localSDP->media)
  {
    if (media.type == "video" && (media.flagAttributes.empty()
                                  || media.flagAttributes.at(0) == A_SENDRECV
                                  || media.flagAttributes.at(0) == A_RECVONLY))
    {
      window_.addVideoStream(sessionID);
    }
  }

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
  removeSession(sessionID, "They cancelled", true);
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
  sip_.acceptCall(sessionID);
}


void KvazzupController::userRejectsCall(uint32_t sessionID)
{
  Logger::getLogger()->printNormal(this, "We reject");
  sip_.rejectCall(sessionID);
  removeSession(sessionID, "Rejected", true);
}


void KvazzupController::userCancelsCall(uint32_t sessionID)
{
  Logger::getLogger()->printNormal(this, "We cancel our call");
  sip_.cancelCall(sessionID);
  removeSession(sessionID, "Cancelled", true);
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
    window_.removeParticipant(sessionID);
  }
  else
  {
    window_.removeWithMessage(sessionID, message, temporaryMessage);
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
