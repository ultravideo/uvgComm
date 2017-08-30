#pragma once

#include "mediamanager.h"
#include "callnegotiation.h"
#include "callwindow.h"
#include "participantinterface.h"

#include <QObject>

struct SDPMessageInfo;
class StatisticsInterface;

class CallManager : public QObject, public ParticipantInterface
{
  Q_OBJECT
public:
  CallManager();

  void init();
  void uninit();

  // participant interface funtions used to start a call or a chat.
  virtual void callToParticipant(QString name, QString username, QString ip);
  virtual void chatWithParticipant(QString name, QString username, QString ip);

public slots:  

  // reaction to user GUI interactions
  void updateSettings();
  void micState();
  void cameraState();
  void endTheCall();
  void windowClosed();
  void acceptCall(QString callID);
  void rejectCall(QString callID);

  // call state changes view negotiation slots
  void incomingCall(QString callID, QString caller);
  void callOurselves(QString callID, std::shared_ptr<SDPMessageInfo> info);
  void callNegotiated(QString callID, std::shared_ptr<SDPMessageInfo> peerInfo,
                     std::shared_ptr<SDPMessageInfo> localInfo);
  void callRinging(QString callID);
  void callRejected(QString callID);
  void callEnded(QString callID, QString ip);

private:

  void createParticipant(QString& callID, std::shared_ptr<SDPMessageInfo> peerInfo,
                         const std::shared_ptr<SDPMessageInfo> localInfo,
                         VideoWidget *videoWidget,
                         StatisticsInterface* stats);

  MediaManager media_; // Media processing and delivery
  CallNegotiation callNeg_; // SIP
  CallWindow window_; // GUI

  StatisticsInterface* stats_;
};
