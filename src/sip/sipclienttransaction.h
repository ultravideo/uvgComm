#pragma once

#include "siptypes.h"

#include <QTimer>
#include <QObject>

class SIPTransactionUser;

class SIPClientTransaction : public QObject
{
  Q_OBJECT
public:
  SIPClientTransaction();

  void init(SIPTransactionUser* tu, uint32_t sessionID);

  bool waitingResponse(RequestType requestType)
  {
    return ongoingTransactionType_ == requestType
        && requestType != SIP_UNKNOWN_REQUEST;
  }

  void getRequestMessageInfo(RequestType type,
                             std::shared_ptr<SIPMessageInfo> &outMessage);

  //processes incoming response. Part of our client transaction
  // returns whether we should destroy the dialog
  bool processResponse(SIPResponse& response);
  void wrongResponseDestination();
  void malformedResponse();
  void responseIsError();

  bool startCall();
  void endCall();
  void registerToServer();

  void connectionReady(bool ready);

signals:
  // send messages to other end
  void sendRequest(uint32_t sessionID, RequestType type);

private slots:
  void requestTimeOut();

private:
  void requestSender(RequestType type);
  bool goodResponse(); // use this to filter out untimely/duplicate responses

  // used to determine what type of request the response is for
  RequestType ongoingTransactionType_;

  // TODO: Probably need to record the whole message and check that the details are ok.

  QTimer requestTimer_;
  bool connected_;

  uint32_t sessionID_;

  // waiting to be sent once the connection has been opened
  RequestType pendingRequest_;

  SIPTransactionUser* transactionUser_;
};
