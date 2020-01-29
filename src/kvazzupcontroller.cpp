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
  window_.init(this);
  window_.show();
  stats_ = window_.createStatsWindow();

  sip_.init(this, stats_);

  QSettings settings("kvazzup.ini", QSettings::IniFormat);
  int autoConnect = settings.value("sip/AutoConnect").toInt();

  if(autoConnect == 1)
  {
    sip_.bindToServer();
  }

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
                   this, &KvazzupController::abortCall);
  media_.init(window_.getViewFactory(), stats_);
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
  qDebug() << "Session Initiation," << metaObject()->className()
           << ": Start Call," << metaObject()->className()
           << ": Initiated call starting to" << con.realName;

  return sip_.startCall(con);
}

uint32_t KvazzupController::chatWithParticipant(QString name, QString username,
                                                QString ip)
{
  qDebug() << "Chatting," << metaObject()->className()
           << ": Chatting with:" << name
           << '(' << username << ") at ip:" << ip << ": Chat not implemented yet";

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
    qDebug() << "Incoming call," << metaObject()->className()
             << ": ERROR: Overwriting and existing session in the Kvazzup Core!";
  }

  QSettings settings("kvazzup.ini", QSettings::IniFormat);
  int autoAccept = settings.value("local/Auto-Accept").toInt();
  if(autoAccept == 1)
  {
    qDebug() << "Incoming call," << metaObject()->className()
             << ": Incoming call auto-accepted!";
    userAcceptsCall(sessionID);
    states_[sessionID] = CALLNEGOTIATING;
    return true;
  }
  else
  {
    window_.displayIncomingCall(sessionID, caller);
    states_[sessionID] = CALLRINGINGWITHUS;
  }
  return false;
}

void KvazzupController::callRinging(uint32_t sessionID)
{
  if(states_.find(sessionID) != states_.end() && states_[sessionID] == CALLINGTHEM)
  {
    qDebug() << "Ringing," << metaObject()->className() << ": Our call is ringing!";
    window_.displayRinging(sessionID);
    states_[sessionID] = CALLRINGINWITHTHEM;
  }
  else
  {
    qDebug() << "Ringing," << metaObject()->className()
             << ": PEER ERROR: Got call ringing for nonexisting call:" << sessionID;
  }
}

void KvazzupController::peerAccepted(uint32_t sessionID)
{
  if(states_.find(sessionID) != states_.end())
  {
    if(states_[sessionID] == CALLRINGINWITHTHEM || states_[sessionID] == CALLINGTHEM)
    {
      printDebug(DEBUG_NORMAL, this, DC_ACCEPT, "They accepted our call!");
      states_[sessionID] = CALLNEGOTIATING;
    }
    else
    {
      printDebug(DEBUG_PEER_ERROR, this, DC_ACCEPT,
                 "Got an accepted call even though we have not yet called them!");
    }
  }
  else
  {
    printDebug(DEBUG_PROGRAM_ERROR, this, DC_ACCEPT,
               "Peer accepted a session which is not in Core.");
  }
}

void KvazzupController::peerRejected(uint32_t sessionID)
{
  if(states_.find(sessionID) != states_.end())
  {
    if(states_[sessionID] == CALLRINGINWITHTHEM)
    {
      qDebug() << "Rejection," << metaObject()->className()
               << ": Our call has been rejected!";
      removeSession(sessionID);
    }
    else
    {
      qDebug() << "Rejection," << metaObject()->className()
               << ": PEER ERROR: Got reject when we weren't calling them:"
               << states_[sessionID];
    }
  }
  else
  {
    qDebug() << "Rejection," << metaObject()->className()
             << ": PEER ERROR: Got reject for nonexisting call:" << sessionID;
    qDebug() << "Rejection," << metaObject()->className()
             << ": Number of ongoing sessions:" << states_.size();
  }
}


void KvazzupController::iceCompleted(quint32 sessionID)
{
  startCall(sessionID, true);
}


void KvazzupController::callNegotiated(uint32_t sessionID)
{
  startCall(sessionID, false);
}


void KvazzupController::startCall(uint32_t sessionID, bool iceNominationComplete)
{
  if (iceNominationComplete || !settingEnalbled("sip/ice"))
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

        if (!settingEnalbled("sip/conference") || states_.size() == 1)
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
        qDebug() << "Negotiation," << metaObject()->className()
                 << ": PEER ERROR: Got call successful negotiation "
                    "even though we are not there yet:"
                 << states_[sessionID];
      }
    }
    else
    {
      qDebug() << "Negotiation," << metaObject()->className()
               << ": ERROR: This session does not exist in Call manager";
    }
  }
}


void KvazzupController::abortCall(uint32_t sessionID)
{
  // TODO: Tell sip manager to send an error for ICE
  endCall(sessionID);
}


void KvazzupController::createSingleCall(uint32_t sessionID)
{
  qDebug() << "Negotiation," << metaObject()->className()
           << ": Call has been agreed upon with peer:" << sessionID;

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
    printDebug(DEBUG_PROGRAM_ERROR, this, DC_ADD_MEDIA,
               "Failed to get SDP. Error should be detected earlier.");
    return;
  }

  media_.addParticipant(sessionID, remoteSDP, localSDP, sendports);
  states_[sessionID] = CALLONGOING;
}

void KvazzupController::setupConference()
{
  // TODO
}


void KvazzupController::callNegotiationFailed(uint32_t sessionID)
{
  // TODO: display a proper error for the user
  peerRejected(sessionID);
}

void KvazzupController::cancelIncomingCall(uint32_t sessionID)
{
  // TODO: display a proper message to the user that peer has cancelled their call
  removeSession(sessionID);
}

void KvazzupController::endCall(uint32_t sessionID)
{
  if(states_.find(sessionID) != states_.end()
     && states_[sessionID] == CALLONGOING)
  {
    media_.removeParticipant(sessionID);
  }
  removeSession(sessionID);
}

void KvazzupController::registeredToServer()
{
  qDebug() << "Core," << metaObject()->className()
           << ": Got info, that we have been registered to SIP server.";
  // TODO: indicate to user in some small detail
}

void KvazzupController::registeringFailed()
{
  qDebug() << "Core," << metaObject()->className() << ": Failed to register";
  // TODO: indicate error to user
}

void KvazzupController::updateSettings()
{
  media_.updateSettings();
  //sip_.updateSettings(); // for blocking list
}

void KvazzupController::userAcceptsCall(uint32_t sessionID)
{
  qDebug() << "Core," << metaObject()->className() << ": Sending accept";
  sip_.acceptCall(sessionID);
  states_[sessionID] = CALLNEGOTIATING;
}

void KvazzupController::userRejectsCall(uint32_t sessionID)
{
  qDebug() << "Core," << metaObject()->className() << ": We have rejected their call";
  sip_.rejectCall(sessionID);
  removeSession(sessionID);
}

void KvazzupController::userCancelsCall(uint32_t sessionID)
{
  qDebug() << "Core," << metaObject()->className() << ": We have cancelled our call";
  sip_.cancelCall(sessionID);
  removeSession(sessionID);
}

void KvazzupController::endTheCall()
{
  qDebug() << "Core," << metaObject()->className()
           << ": End all call," << metaObject()->className()
           << ": End all calls button pressed";

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
