#include "callmanager.h"

#include "statisticsinterface.h"

#include "globalsdpstate.h"

#include <QHostAddress>


CallManager::CallManager():
    media_(),
    sip_(),
    window_(nullptr)
{}

void CallManager::init()
{
  window_.init(this);
  window_.show();

  sip_.init(this);

  // register the GUI signals indicating GUI changes to be handled approrietly in a system wide manner
  QObject::connect(&window_, SIGNAL(settingsChanged()), this, SLOT(updateSettings()));
  QObject::connect(&window_, SIGNAL(micStateSwitch()), this, SLOT(micState()));
  QObject::connect(&window_, SIGNAL(cameraStateSwitch()), this, SLOT(cameraState()));
  QObject::connect(&window_, SIGNAL(endCall()), this, SLOT(endTheCall()));
  QObject::connect(&window_, SIGNAL(closed()), this, SLOT(windowClosed()));

  QObject::connect(&window_, &CallWindow::callAccepted, this, &CallManager::userAcceptsCall);
  QObject::connect(&window_, &CallWindow::callRejected, this, &CallManager::userRejectsCall);
  QObject::connect(&window_, &CallWindow::callCancelled, this, &CallManager::userCancelsCall);

  stats_ = window_.createStatsWindow();

  media_.init(window_.getViewFactory(), stats_);

  QObject::connect(&stun_, SIGNAL(addressReceived(QHostAddress)), this, SLOT(stunAddress(QHostAddress)));
  QObject::connect(&stun_, SIGNAL(error()), this, SLOT(noStunAddress()));
  //stun_.wantAddress("stun.l.google.com");
}

void CallManager::stunAddress(QHostAddress address)
{
  qDebug() << "Our stun address:" << address;
}

void CallManager::noStunAddress()
{
  qDebug() << "Could not get STUN address";
}

void CallManager::uninit()
{
  endTheCall();
  sip_.uninit();
  media_.uninit();
}

void CallManager::windowClosed()
{
  uninit();
}

void CallManager::callToParticipant(QString name, QString username, QString ip)
{
  QString ip_str = ip;

  Contact con;
  con.remoteAddress = ip_str;
  con.realName = name;
  con.username = username;
  con.proxyConnection = false;

  QList<Contact> list;
  list.append(con);

  //start negotiations for this connection

  QList<uint32_t> sessionIDs = sip_.startCall(list);

  qDebug() << "Started" << sessionIDs.size() << "calls:" << sessionIDs;

  for(auto sessionID : sessionIDs)
  {
    window_.displayOutgoingCall(sessionID, name);
    if(states_.find(sessionID) == states_.end())
    {
      states_[sessionID] = CALLINGTHEM;
    }
  }
}

void CallManager::chatWithParticipant(QString name, QString username, QString ip)
{
  qDebug() << "Chatting with:" << name << '(' << username << ") at ip:" << ip << ": Chat not implemented yet";
}

bool CallManager::incomingCall(uint32_t sessionID, QString caller)
{
  if(states_.find(sessionID) != states_.end())
  {
    qDebug() << "ERROR: Overwriting and existing session in CallManager!";
  }

  QSettings settings("kvazzup.ini", QSettings::IniFormat);
  int autoAccept = settings.value("local/Auto-Accept").toInt();
  if(autoAccept == 1)
  {
    qDebug() << "Incoming call auto-accpeted!";
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

void CallManager::callRinging(uint32_t sessionID)
{
  if(states_.find(sessionID) != states_.end() && states_[sessionID] == CALLINGTHEM)
  {
    qDebug() << "Our call is ringing!";
    window_.displayRinging(sessionID);
    states_[sessionID] = CALLRINGINWITHTHEM;
  }
  else
  {
    qDebug() << "PEER ERROR: Got call ringing for nonexisting call:" << sessionID;
  }
}

void CallManager::peerAccepted(uint32_t sessionID)
{
  if(states_.find(sessionID) != states_.end())
  {
    if(states_[sessionID] == CALLRINGINWITHTHEM || true)
    {
      qDebug() << "They accepted our call!";
      states_[sessionID] = CALLNEGOTIATING;
    }
    else
    {
      qDebug() << "PEER ERROR: Got an accepted call even though we have not yet called them!";
    }
  }
  else
  {
    qDebug() << "ERROR: Peer accepted a session which is not in Call manager";
  }
}

void CallManager::peerRejected(uint32_t sessionID)
{
  if(states_.find(sessionID) != states_.end())
  {
    if(states_[sessionID] == CALLRINGINWITHTHEM)
    {
      qDebug() << "Our call has been rejected!";
      removeSession(sessionID);
    }
    else
    {
      qDebug() << "PEER ERROR: Got reject when we weren't calling them:" << states_[sessionID];
    }
  }
  else
  {
    qDebug() << "PEER ERROR: Got reject for nonexisting call:" << sessionID;
    qDebug() << "Number of ongoing sessions:" << states_.size();
  }
}

void CallManager::callNegotiated(uint32_t sessionID)
{
  if(states_.find(sessionID) != states_.end())
  {
    if(states_[sessionID] == CALLNEGOTIATING || true)
    {
      qDebug() << "Call has been agreed upon with peer:" << sessionID;

      window_.addVideoStream(sessionID);

      std::shared_ptr<SDPMessageInfo> localSDP;
      std::shared_ptr<SDPMessageInfo> remoteSDP;

      sip_.getSDPs(sessionID,
                   localSDP,
                   remoteSDP);

      if(localSDP == nullptr || remoteSDP == nullptr)
      {
        return;
      }

      media_.addParticipant(sessionID, remoteSDP, localSDP);
      states_[sessionID] = CALLONGOING;
    }
    else
    {
       qDebug() << "PEER ERROR: Got call successful negotiation even though we are not there yet:" << states_[sessionID];
    }
  }
  else
  {
     qDebug() << "ERROR: This session does not exist in Call manager";
  }
}

void CallManager::callNegotiationFailed(uint32_t sessionID)
{
  // TODO: display a proper error for the user
  peerRejected(sessionID);
}

void CallManager::cancelIncomingCall(uint32_t sessionID)
{
  // TODO: display a proper message to the user that peer has cancelled their call
  removeSession(sessionID);
}

void CallManager::endCall(uint32_t sessionID)
{
  if(states_.find(sessionID) != states_.end()
     && states_[sessionID] == CALLONGOING)
  {
    media_.removeParticipant(sessionID);
  }
  removeSession(sessionID);
}

void CallManager::registeredToServer()
{
  qDebug() << "Got info, that we have been registered to SIP server.";
  // TODO: indicate to user in some small detail
}

void CallManager::registeringFailed()
{
  qDebug() << "Failed to register";
  // TODO: indicate error to user
}

void CallManager::updateSettings()
{
  media_.updateSettings();
  //sip_.updateSettings(); // for blocking list
}

void CallManager::userAcceptsCall(uint32_t sessionID)
{
  qDebug() << "Sending accept";
  sip_.acceptCall(sessionID);
  states_[sessionID] = CALLNEGOTIATING;
}

void CallManager::userRejectsCall(uint32_t sessionID)
{
  qDebug() << "We have rejected their call";
  sip_.rejectCall(sessionID);
  removeSession(sessionID);
}

void CallManager::userCancelsCall(uint32_t sessionID)
{
  qDebug() << "We have cancelled our call";
  sip_.cancelCall(sessionID);
  removeSession(sessionID);
}

void CallManager::endTheCall()
{
  qDebug() << "Ending all calls";
  sip_.endAllCalls();
  media_.endAllCalls();
  window_.clearConferenceView();

  states_.clear();
}

void CallManager::micState()
{
  window_.setMicState(media_.toggleMic());
}

void CallManager::cameraState()
{
  window_.setCameraState(media_.toggleCamera());
}

void CallManager::removeSession(uint32_t sessionID)
{
  window_.removeParticipant(sessionID);

  auto it = states_.find(sessionID);
  if(it != states_.end())
  {
    states_.erase(it);
  }
}
