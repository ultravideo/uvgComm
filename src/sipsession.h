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

  // send messages to other end
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
