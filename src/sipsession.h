#pragma once

#include "siptypes.h"

#include <memory>

/* This class maintains the state of the SIP call and
 * selects the correct SIP response to any given situation.
 * All SIP requests and responses are iniated here.
 * Class is responsible for maintaining Call-ID and tags
 * to identify this call. Each call should have their own call
 * session.
 */

class SIPSession
{
public:
  SIPSession();

  void init(uint32_t sessionID);

  void setOurSession(bool ourSession)
  {
    ourSession_ = ourSession;
  }

  // generates callID if we wanted to start a call
  void generateCallID(QString localAddress);

  // these will provide both message and session structs, routing will be empty
  std::shared_ptr<SIPMessageInfo> getRequestInfo(RequestType type);
  std::shared_ptr<SIPMessageInfo> getResponseInfo(RequestType ongoingTransaction);

  bool processRequest(std::shared_ptr<SIPSessionInfo> session);
  bool processResponse(std::shared_ptr<SIPSessionInfo> session);

  // forbid copy and assignment
  SIPSession(const SIPSession& copied) = delete;
  SIPSession& operator=(SIPSession const&) = delete;

private:

  std::shared_ptr<SIPMessageInfo> generateMessage(RequestType originalRequest);

  QString localTag_;
  QString remoteTag_;
  QString callID_;
  uint32_t sessionID_;
  uint32_t cSeq_;

  bool registered_;
  bool ourSession_; // is callID our or theirs
};
