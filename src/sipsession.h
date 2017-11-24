#pragma once

#include "siptypes.h"

/* The main function of this class is to keep track of the call state.
 * Pass all user requests through this class which are checked for legality
 * and reacted view requests and responses.
 *
 * Is responsible for:
 * 1) check if this is the correct session
 * 2) check from incoming messages is legal at this point.
 * 3) modify call state when user wants to modify the call and forward the request
 */

// where is the checking of request/response type and the reaction?

class SIPSession
{
public:
  SIPSession();

  void setStateInfo(SIPStateInfo state, uint32_t sessionID);

  // checks that the incoming message belongs to this session
  bool correctSession(SIPStateInfo state) const;

  // processes incoming request
  void processRequest(RequestType request, const SIPStateInfo& messageState);

  //processes incoming response
  void processResponse(ResponseType response, const SIPStateInfo& messageState);

  void startCall();
  void endCall();
  void registerToServer();

  void sendMessage(QString message);

signals:

  void callRinging(uint32_t sessionID);
  void callAccepted(uint32_t sessionID);
  void callFailed(uint32_t sessionID);
  void callDeclined(uint32_t sessionID);
  void callEnded(uint32_t sessionID);

  void registerSucceeded(uint32_t sessionID);
  void registerFailed(uint32_t sessionID);

  void sendRequest(uint32_t sessionID, RequestType type, const SIPStateInfo& state);
  void sendResponse(uint32_t sessionID, ResponseType type, const SIPStateInfo& state);

private:

  SIPStateInfo state_;

  uint32_t sessionID_;

  RequestType ongoingTransactionType_;
};
