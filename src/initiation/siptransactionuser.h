#pragma once

#include <QString>
#include <memory>

#include "initiation/negotiation/sdptypes.h"

/* Defines funcions implemented by transaction user (TU). Transaction layer call this
 * to let the program (transaction user) know of changes or messages in SIP Transaction.
 */

// TODO: I've been thinking that it would be simpler to just pass the SIP messages as they
// arrive to the rest of the application which can then react to them as they please.
// The current implementation is more complicated in both ends, but especially in the SIP
// section.

class SIPTransactionUser
{
 public:

  virtual ~SIPTransactionUser(){}

  // the call is ringing
  virtual void callRinging(uint32_t sessionID) = 0;

  // the call has been accepted
  virtual void peerAccepted(uint32_t sessionID) = 0;

  // our or their call has finished negotiating
  virtual void callNegotiated(uint32_t sessionID) = 0;

  // the call has ended
  virtual void endCall(uint32_t sessionID) = 0;

  // some kind of failure has happened and the session is no longer valid
  virtual void failure(uint32_t sessionID, QString error) = 0;

  // we have succesfully registered to the server
  virtual void registeredToServer() = 0;

  // our registeration failed.
  virtual void registeringFailed() = 0;
};
