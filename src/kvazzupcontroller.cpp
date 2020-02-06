#include "kvazzupcontroller.h"

#include "statisticsinterface.h"

#include "common.h"

#include <QSettings>
#include <QHostAddress>


KvazzupController::KvazzupController():
    media_(),
    sip_(),
    window_(nullptr)
{}

void KvazzupController::init()
{
  printImportant(this, "Kvazzup initiation Started");
  window_.init(this);
  window_.show();
  stats_ = window_.createStatsWindow();

  sip_.init(this, stats_, window_.getStatusView());


  // register the GUI signals indicating GUI changes to be handled
  // approrietly in a system wide manner
  QObject::connect(&window_, SIGNAL(settingsChanged()), this, SLOT(updateSettings()));
  QObject::connect(&window_, SIGNAL(micStateSwitch()), this, SLOT(micState()));
  QObject::connect(&window_, SIGNAL(cameraStateSwitch()), this, SLOT(cameraState()));
  QObject::connect(&window_, SIGNAL(endCall()), this, SLOT(endTheCall()));
  QObject::connect(&window_, SIGNAL(closed()), this, SLOT(windowClosed()));

  QObject::connect(&window_, &CallWindow::callAccepted,
                   this, &KvazzupController::userAcceptsCall);
  QObject::connect(&window_, &CallWindow::callRejected,
                   this, &KvazzupController::userRejectsCall);
  QObject::connect(&window_, &CallWindow::callCancelled,
                   this, &KvazzupController::userCancelsCall);

  QObject::connect(&sip_, &SIPManager::nominationSucceeded,
                   this, &KvazzupController::iceCompleted);
  QObject::connect(&sip_, &SIPManager::nominationFailed,
                   this, &KvazzupController::iceFailed);
  media_.init(window_.getViewFactory(), stats_);

  printImportant(this, "Kvazzup initiation finished");
}

void KvazzupController::uninit()
{
  sip_.uninit();
  media_.uninit();
}

void KvazzupController::windowClosed()
{
  uninit();
}

uint32_t KvazzupController::callToParticipant(QString name, QString username, QString ip)
{
  QString ip_str = ip;

  Contact con;
  con.remoteAddress = ip_str;
  con.realName = name;
  con.username = username;

  //start negotiations for this connection

  printNormal(this, "Starting call with contact", {"Contact"}, {con.realName});

  return sip_.startCall(con);
}

uint32_t KvazzupController::chatWithParticipant(QString name, QString username,
                                                QString ip)
{
  printDebug(DEBUG_NORMAL, this, "Starting a chat with contact",
            {"ip", "Name", "Username"}, {ip, name, username});
  return 0;
}

void KvazzupController::outgoingCall(uint32_t sessionID, QString callee)
{
  window_.displayOutgoingCall(sessionID, callee);
  if(states_.find(sessionID) == states_.end())
  {
    states_[sessionID] = CALLINGTHEM;
  }
}

bool KvazzupController::incomingCall(uint32_t sessionID, QString caller)
{
  if(states_.find(sessionID) != states_.end())
  {
    printProgramError(this, "Incoming call is overwriting an existing session!");
  }

  QSettings settings("kvazzup.ini", QSettings::IniFormat);
  int autoAccept = settings.value("local/Auto-Accept").toInt();
  if(autoAccept == 1)
  { 
    printNormal(this, "Incoming call auto-accepted");

    userAcceptsCall(sessionID);
    states_[sessionID] = CALLNEGOTIATING;
    return true;
  }
  else
  {
    printNormal(this, "Showing incoming call");
    window_.displayIncomingCall(sessionID, caller);
    states_[sessionID] = CALLRINGINGWITHUS;
  }
  return false;
}

void KvazzupController::callRinging(uint32_t sessionID)
{
  if(states_.find(sessionID) != states_.end() && states_[sessionID] == CALLINGTHEM)
  {
    printNormal(this, "Our call is ringing");
    window_.displayRinging(sessionID);
    states_[sessionID] = CALLRINGINWITHTHEM;
  }
  else
  {
    printPeerError(this, "Got call ringing for nonexisting call",
                  {"SessionID"}, {sessionID});
  }
}

void KvazzupController::peerAccepted(uint32_t sessionID)
{
  if(states_.find(sessionID) != states_.end())
  {
    if(states_[sessionID] == CALLRINGINWITHTHEM || states_[sessionID] == CALLINGTHEM)
    {
      printImportant(this, "They accepted our call!");
      states_[sessionID] = CALLNEGOTIATING;
    }
    else
    {
      printPeerError(this, "Got an accepted call even though we have not yet called them!");
    }
  }
  else
  {
    printPeerError(this, "Peer accepted a session which is not in Controller.");
  }
}

void KvazzupController::peerRejected(uint32_t sessionID)
{
  if(states_.find(sessionID) != states_.end())
  {
    if(states_[sessionID] == CALLRINGINWITHTHEM)
    {
      printImportant(this, "Our call has been rejected!");
      removeSession(sessionID);
    }
    else
    {
      printPeerError(this, "Got reject when we weren't calling them", "SessionID", {sessionID});
    }
  }
  else
  {
    printPeerError(this, "Got reject for nonexisting call", "SessionID", {sessionID});
    printPeerError(this, "", "Ongoing sessions", {QString::number(states_.size())});
  }
}


void KvazzupController::iceCompleted(quint32 sessionID)
{
  printNormal(this, "ICE has been successfully completed",
            {"SessionID"}, {sessionID});
  startCall(sessionID, true);
}


void KvazzupController::callNegotiated(uint32_t sessionID)
{
  printNormal(this, "Call negotiated");
  startCall(sessionID, false);
}


void KvazzupController::startCall(uint32_t sessionID, bool iceNominationComplete)
{
  if (iceNominationComplete || !settingEnabled("sip/ice"))
  {
    if(states_.find(sessionID) != states_.end())
    {
      if(states_[sessionID] == CALLNEGOTIATING || states_[sessionID] == CALLONGOING)
      {
        if (states_[sessionID] == CALLONGOING)
        {
          // we have to remove previous media so we do not double them.
          media_.removeParticipant(sessionID);
          window_.removeParticipant(sessionID);
        }

        if (!settingEnabled("sip/conference") || states_.size() == 1)
        {
          createSingleCall(sessionID);
        }
        else
        {
          setupConference();
        }
      }
      else
      {
        printPeerError(this, "Got call successful negotiation "
                             "even though we are not there yet, "
                             "State",
                             {states_[sessionID]});
      }
    }
    else
    {
      printProgramError(this, "The call state does not exist when starting the call.",
                        {"SessionID"}, {sessionID});
    }
  }

  printNormal(this, "Waiting for the ICE to finish before starting the call.");
}


void KvazzupController::iceFailed(uint32_t sessionID)
{
  printError(this, "ICE has failed");

  // TODO: Tell sip manager to send an error for ICE
  printUnimplemented(this, "Send SIP error code for ICE failure");
  endCall(sessionID);
}


void KvazzupController::createSingleCall(uint32_t sessionID)
{
  printNormal(this, "Call has been agreed upon with peer.",
              "SessionID", {sessionID});

  std::shared_ptr<SDPMessageInfo> localSDP;
  std::shared_ptr<SDPMessageInfo> remoteSDP;
  QList<uint16_t> sendports;

  sip_.getSDPs(sessionID,
               localSDP,
               remoteSDP,
               sendports);

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
    printError(this, "Failed to get SDP. Error should be detected earlier.");
    return;
  }

  media_.addParticipant(sessionID, remoteSDP, localSDP, sendports);
  states_[sessionID] = CALLONGOING;
}

void KvazzupController::setupConference()
{
  // TODO: conferencing
  printUnimplemented(this, "Setup conference.");
}


void KvazzupController::callNegotiationFailed(uint32_t sessionID)
{
  // TODO: display a proper message to the user the negotiation has failed
  printUnimplemented(this, "Tell user that the negotiation has failed.");
  peerRejected(sessionID);
}


void KvazzupController::cancelIncomingCall(uint32_t sessionID)
{
  // TODO: display a proper message to the user that peer has cancelled their call
  printUnimplemented(this, "Tell user that the call has been cancelled.");
  removeSession(sessionID);
}


void KvazzupController::endCall(uint32_t sessionID)
{
  printNormal(this, "Ending the call", {"SessionID"}, {sessionID});
  if (states_.find(sessionID) != states_.end() &&
      states_[sessionID] == CALLONGOING)
  {
    media_.removeParticipant(sessionID);
  }
  removeSession(sessionID);
}


void KvazzupController::registeredToServer()
{
  printImportant(this, "We have been registered to a SIP server.");
  // TODO: indicate to user in some small detail
}


void KvazzupController::registeringFailed()
{
  printError(this, "Failed to register to a SIP server.");
  // TODO: indicate error to user
}


void KvazzupController::updateSettings()
{
  media_.updateSettings();
  sip_.updateSettings();
}


void KvazzupController::userAcceptsCall(uint32_t sessionID)
{
  printNormal(this, "We accept");
  sip_.acceptCall(sessionID);
  states_[sessionID] = CALLNEGOTIATING;
}


void KvazzupController::userRejectsCall(uint32_t sessionID)
{
  printNormal(this, "We reject");
  sip_.rejectCall(sessionID);
  removeSession(sessionID);
}


void KvazzupController::userCancelsCall(uint32_t sessionID)
{
  printNormal(this, "We cancel our call");
  sip_.cancelCall(sessionID);
  removeSession(sessionID);
}


void KvazzupController::endTheCall()
{
  printNormal(this, "We end the call");

  sip_.endAllCalls();
  media_.endAllCalls();
  window_.clearConferenceView();

  states_.clear();
}


void KvazzupController::micState()
{
  window_.setMicState(media_.toggleMic());
}


void KvazzupController::cameraState()
{
  window_.setCameraState(media_.toggleCamera());
}


void KvazzupController::removeSession(uint32_t sessionID)
{
  window_.removeParticipant(sessionID);

  auto it = states_.find(sessionID);
  if(it != states_.end())
  {
    states_.erase(it);
  }
}
