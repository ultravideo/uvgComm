#pragma once

#include "mediamanager.h"
#include "sip/siptransactions.h"
#include "gui/callwindow.h"
#include "participantinterface.h"
#include "sip/siptransactionuser.h"

#include <QObject>

#include <map>


/* This is the core of the program and thus should control what the other modules
 * do and in which order when something needs to be done.
 */


struct SDPMessageInfo;
class StatisticsInterface;

class KvazzupCore : public QObject, public ParticipantInterface, public SIPTransactionUser
{
  Q_OBJECT
public:
  KvazzupCore();

  void init();
  void uninit();

  // participant interface funtions used to start a call or a chat.
  virtual void callToParticipant(QString name, QString username, QString ip);
  virtual void chatWithParticipant(QString name, QString username, QString ip);

  // Call Control Interface used by SIP transaction
  virtual void outgoingCall(uint32_t sessionID, QString callee);
  virtual bool incomingCall(uint32_t sessionID, QString caller);
  virtual void callRinging(uint32_t sessionID);
  virtual void peerAccepted(uint32_t sessionID);
  virtual void peerRejected(uint32_t sessionID);
  virtual void callNegotiated(uint32_t sessionID);
  virtual void callNegotiationFailed(uint32_t sessionID);
  virtual void cancelIncomingCall(uint32_t sessionID);
  virtual void endCall(uint32_t sessionID);
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

private:

  void removeSession(uint32_t sessionID);

  struct PeerState
  {
    bool viewAdded;
    bool mediaAdded;
  };

  // Call state is a non-dependant way
  enum CallState{CALLRINGINGWITHUS = 0, CALLINGTHEM = 1, CALLRINGINWITHTHEM = 2,
                 CALLNEGOTIATING = 3, CALLONGOING = 4};
  std::map<uint32_t, CallState> states_;

  MediaManager media_; // Media processing and delivery
  SIPTransactions sip_; // SIP
  CallWindow window_; // GUI

  StatisticsInterface* stats_;
};
