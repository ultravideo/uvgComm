#pragma once

#include <QString>
#include <memory>

class CallControlInterface
{
 public:

  virtual ~CallControlInterface(){}

  // somebody is trying to call us
  virtual void incomingCall(uint32_t sessionID, QString caller) = 0;

  // the call is ringing
  virtual void callRinging(uint32_t sessionID) = 0;

  // the call has been rejected
  virtual void callRejected(uint32_t sessionID) = 0;

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
