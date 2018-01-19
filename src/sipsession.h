#pragma once

#include "siptypes.h"

#include <QTimer>
#include <QMUtex>
#include <QObject>

#include <memory>

/* The main function of this class is to keep track of the call state.
 * Pass all user requests and SIP messages through this class which are checked for legality.
 * This class is also responsible for sending correct request or response. This class also
 * does the translation between SIP CallID and sessionID.
 *
 * Is responsible for:
 * 1) check if this is the correct session
 * 2) check from incoming messages is legal at this point.
 * 3) modify call state when user wants to modify the call and forward the request
 *
 */

// where is the checking of request/response type and the reaction?

enum CallState {CALL_INACTIVE, CALL_NEGOTIATING, CALL_ACTIVE};

class SIPSession : public QObject
{
  Q_OBJECT

public:
  SIPSession();

  void init(uint32_t sessionID);

  std::shared_ptr<SIPSessionInfo> getSessionInfo();

  SIPMessage generateMessage(RequestType originalRequest);

  // checks that the incoming message belongs to this session
  bool correctSession(const SIPSessionInfo& session) const;

  // processes incoming request
  void processRequest(RequestType request,
                      const SIPSessionInfo& session,
                      const SIPMessage& messageInfo);

  //processes incoming response
  void processResponse(ResponseType response,
                       const SIPSessionInfo& session,
                       const SIPMessage& messageInfo);

  bool startCall();
  void endCall();
  void registerToServer();

  // forbid copy and assignment
  SIPSession(const SIPSession& copied) = delete;
  SIPSession& operator=(SIPSession const&) = delete;

  private slots:

  void requestTimeOut();

signals:

  // notify the user
  void callRinging(uint32_t sessionID);
  void callAccepted(uint32_t sessionID);
  void callFailed(uint32_t sessionID);
  void callDeclined(uint32_t sessionID);
  void callEnded(uint32_t sessionID);

  void registerSucceeded(uint32_t sessionID);
  void registerFailed(uint32_t sessionID);

  void sendRequest(uint32_t sessionID, RequestType type);
  void sendResponse(uint32_t sessionID, ResponseType type);

private:

  void setSessionInfo(std::shared_ptr<SIPSessionInfo> info, uint32_t sessionID);
  void requestSender(RequestType type);

  QString generateRandomString(uint32_t length);

  std::shared_ptr<SIPSessionInfo> session_;

  uint32_t sessionID_;
  uint32_t cSeq_;

  // used to check if the received response is for our request
  RequestType ongoingTransactionType_;
  CallState state_;

  bool registered_;

  QTimer timeoutTimer_;
};
