#pragma once

#include <QString>
#include <memory>

#include "sip/sdptypes.h"

// defines funcions implemented by transaction user (TU). Transaction layer call this
// to let the program (transactino user) know of changes or messages in SIP Transaction.


class SIPTransactionUser
{
 public:

  virtual ~SIPTransactionUser(){}

  // We are calling someone
  virtual void outgoingCall(uint32_t sessionID, QString callee) = 0;

  // somebody is trying to call us
  // returns true if this is auto-accepted
  virtual bool incomingCall(uint32_t sessionID, QString caller) = 0;

  // the call is ringing
  virtual void callRinging(uint32_t sessionID) = 0;

  // the call has been accepted
  virtual void peerAccepted(uint32_t sessionID) = 0;

  // the call has been rejected
  virtual void peerRejected(uint32_t sessionID) = 0;

  // our or their call has finished negotiating
  virtual void callNegotiated(uint32_t sessionID) = 0;

  // the media for this call was not compatible
  virtual void callNegotiationFailed(uint32_t sessionID) = 0;

  // they cancelled the call request
  virtual void cancelIncomingCall(uint32_t sessionID) = 0;

  // the call has ended
  virtual void endCall(uint32_t sessionID) = 0;

  // we have succesfully registered to the server
  virtual void registeredToServer() = 0;

  // our registeration failed.
  virtual void registeringFailed() = 0;
};
