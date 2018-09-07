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

  QObject::connect(&window_, &CallWindow::callAccepted, this, &CallManager::acceptCall);
  QObject::connect(&window_, &CallWindow::callRejected, this, &CallManager::rejectCall);

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

  for(auto sessionID : sessionIDs)
  {
    window_.displayOutgoingCall(sessionID, name);
  }

}

void CallManager::chatWithParticipant(QString name, QString username, QString ip)
{
  qDebug() << "Chatting with:" << name << '(' << username << ") at ip:" << ip << ": Chat not implemented yet";
}

bool CallManager::incomingCall(uint32_t sessionID, QString caller)
{
  QSettings settings("kvazzup.ini", QSettings::IniFormat);
  int autoAccept = settings.value("local/Auto-Accept").toInt();
  if(autoAccept == 1)
  {
    acceptCall(sessionID);
    return true;
  }
  else
  {
    window_.displayIncomingCall(sessionID, caller);
  }
  return false;
}

void CallManager::callRinging(uint32_t sessionID)
{
  qDebug() << "Our call is ringing!";
  window_.displayRinging(sessionID);
}

void CallManager::callRejected(uint32_t sessionID)
{
  qDebug() << "Our call has been rejected!";
  window_.removeParticipant(sessionID);
}

void CallManager::callNegotiated(uint32_t sessionID)
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
}

void CallManager::callNegotiationFailed(uint32_t sessionID)
{
  // TODO: display a proper error for the user
  callRejected(sessionID);
}

void CallManager::cancelIncomingCall(uint32_t sessionID)
{
  // TODO: display a proper message to the user
  window_.removeParticipant(sessionID);
}

void CallManager::endCall(uint32_t sessionID)
{
  media_.removeParticipant(sessionID);
  window_.removeParticipant(sessionID);
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
}

void CallManager::acceptCall(uint32_t sessionID)
{
  qDebug() << "Sending accept";
  sip_.acceptCall(sessionID);
}

void CallManager::rejectCall(uint32_t sessionID)
{
  qDebug() << "We have rejected their call";
  sip_.rejectCall(sessionID);
  window_.removeParticipant(sessionID);
}

void CallManager::endTheCall()
{
  qDebug() << "Ending all calls";
  sip_.endAllCalls();
  media_.endAllCalls();
  window_.clearConferenceView();
}

void CallManager::micState()
{
  window_.setMicState(media_.toggleMic());
}

void CallManager::cameraState()
{
  window_.setCameraState(media_.toggleCamera());
}
