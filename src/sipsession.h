#pragma once

#include "siptypes.h"

#include <QTimer>
#include <QMUtex>
#include <QObject>

/* The main function of this class is to keep track of the call state.
 * Pass all user requests through this class which are checked for legality
 * and reacted view requests and responses.
 *
 * Is responsible for:
 * 1) check if this is the correct session
 * 2) check from incoming messages is legal at this point.
 * 3) modify call state when user wants to modify the call and forward the request
 *
 * This class is responsible for deciding which requests and responses to send.
 */

// where is the checking of request/response type and the reaction?

enum CallState {CALL_INACTIVE, CALL_NEGOTIATING, CALL_ACTIVE};

class SIPSession : public QObject
{
  Q_OBJECT

public:
  SIPSession();

  void setStateInfo(SIPSessionInfo session, uint32_t sessionID);

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

  void startCall();
  void endCall();
  void registerToServer();

  void sendMessage(QString message);

  // forbid copy and assignment
  SIPSession(const SIPSession& copied) = delete;
  SIPSession& operator=(SIPSession const&) = delete;

  private slots:

  void requestTimeOut();

signals:

  void callRinging(uint32_t sessionID);
  void callAccepted(uint32_t sessionID);
  void callFailed(uint32_t sessionID);
  void callDeclined(uint32_t sessionID);
  void callEnded(uint32_t sessionID);

  void registerSucceeded(uint32_t sessionID);
  void registerFailed(uint32_t sessionID);

  void sendRequest(uint32_t sessionID, RequestType type, const SIPSessionInfo& session);
  void sendResponse(uint32_t sessionID, ResponseType type, const SIPSessionInfo& session);

  void timedOut(uint32_t sessionID, RequestType type);

private:

  void requestSender(RequestType type);

  QString generateRandomString(uint32_t length);

  SIPSessionInfo session_;

  uint32_t sessionID_;

  // used to check if the received response is for our request
  RequestType ongoingTransactionType_;
  CallState state_;

  bool registered_;

  QTimer timeoutTimer_;
};
