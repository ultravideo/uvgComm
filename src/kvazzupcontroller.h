#pragma once

#include "media/mediamanager.h"
#include "initiation/sipmanager.h"
#include "initiation/siptransactionuser.h"
#include "ui/gui/callwindow.h"
#include "participantinterface.h"

#include <QObject>

#include <map>


/* This is the main class of the program and thus should control what, in which order
 * and when the other modules do tasks.
 */


struct SDPMessageInfo;
class StatisticsInterface;

class KvazzupController :
    public QObject,
    public ParticipantInterface,
    public SIPTransactionUser
{
  Q_OBJECT
public:
  KvazzupController();

  void init();
  void uninit();

  // participant interface funtions used to start a call or a chat.
  virtual uint32_t callToParticipant(QString name, QString username, QString ip);
  virtual uint32_t chatWithParticipant(QString name, QString username, QString ip);

  // Call Control Interface used by SIP transaction
  virtual void outgoingCall(uint32_t sessionID, QString callee);
  virtual bool incomingCall(uint32_t sessionID, QString caller);
  virtual void callRinging(uint32_t sessionID);
  virtual void peerAccepted(uint32_t sessionID);
  virtual void callNegotiated(uint32_t sessionID);
  virtual void cancelIncomingCall(uint32_t sessionID);
  virtual void endCall(uint32_t sessionID);
  virtual void failure(uint32_t sessionID, QString error);
  virtual void registeredToServer();
  virtual void registeringFailed();

public slots:  

  // reaction to user GUI interactions
  void updateSettings(); // update all the components that use settings
  void micState();       // change mic state
  void cameraState();    // change camera state
  void endTheCall();     // user wants to end the call
  void windowClosed();   // user has closed the window
  void userAcceptsCall(uint32_t sessionID); // user has accepted the incoming call
  void userRejectsCall(uint32_t sessionID); // user has rejected the incoming call
  void userCancelsCall(uint32_t sessionID); // user has rejected the incoming call

  void iceCompleted(quint32 sessionID);
  void iceFailed(quint32 sessionID);

private:
  void startCall(quint32 sessionID, bool iceNominationComplete);
  void removeSession(uint32_t sessionID);

  void createSingleCall(uint32_t sessionID);
  void setupConference();

  struct PeerState
  {
    bool viewAdded;
    bool mediaAdded;
  };

  // Call state is a non-dependant way
  enum CallState {
    CALLRINGINGWITHUS = 0,
    CALLINGTHEM = 1,
    CALLRINGINWITHTHEM = 2,
    CALLNEGOTIATING = 3,
    CALLONGOING = 4
  };

  std::map<uint32_t, CallState> states_;

  MediaManager media_; // Media processing and delivery
  SIPManager sip_; // SIP
  CallWindow window_; // GUI

  StatisticsInterface* stats_;

};
