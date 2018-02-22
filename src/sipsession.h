#pragma once

#include "siptypes.h"

#include <QTimer>
#include <QMUtex>
#include <QObject>

#include <memory>

/* This class maintains the state of the SIP call and
 * selects the correct SIP response to any given situation.
 * All SIP requests and responses are iniated here.
 * Class is responsible for maintaining Call-ID and tags
 * to identify this call. Each call should have their own call
 * session.
 */

enum CallState {CALL_INACTIVE, CALL_NEGOTIATING, CALL_ACTIVE};

class CallControlInterface;

class SIPSession : public QObject
{
  Q_OBJECT

public:
  SIPSession();

  void init(uint32_t sessionID, CallControlInterface* callControl);

  std::shared_ptr<SIPSessionInfo> getRequestInfo();
  std::shared_ptr<SIPSessionInfo> getResponseInfo();

  std::shared_ptr<SIPMessageInfo> generateMessage(RequestType originalRequest);

  bool correctRequest(std::shared_ptr<SIPSessionInfo> session);
  bool correctResponse(std::shared_ptr<SIPSessionInfo> session);

  // processes incoming request
  void processRequest(SIPRequest& request);

  //processes incoming response
  void processResponse(SIPResponse& response);

  bool startCall();
  void endCall();
  void registerToServer();
  void acceptCall();

  void connectionReady(bool ready);

  // messages from other components
  void malformedMessage(); // if parse fails
  void wrongDestination(); // if this is the wrong destination

  // forbid copy and assignment
  SIPSession(const SIPSession& copied) = delete;
  SIPSession& operator=(SIPSession const&) = delete;

  private slots:

  void requestTimeOut();

signals:

  void incomingCall(uint32_t sessionID);
  void cancelIncomingCall(uint32_t sessionID);
  void callStarting(uint32_t sessionID);
  void callEnded(uint32_t sessionID);

  // send messages to other end
  void sendRequest(uint32_t sessionID, RequestType type);
  void sendResponse(uint32_t sessionID, ResponseType type);

private:

  void requestSender(RequestType type);
  void responseSender(ResponseType type);

  QString localTag_;
  QString remoteTag_;
  QString callID_;

  uint32_t sessionID_;
  uint32_t cSeq_;

  // used to check if the received response is for our request
  RequestType ongoingTransactionType_;
  CallState state_;

  bool registered_;

  QTimer timeoutTimer_;

  bool connected_;
  RequestType pendingRequest_;
  ResponseType pendingResponse_;

  CallControlInterface* callControl_;
};
