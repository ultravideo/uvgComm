#pragma once

#include "siptypes.h"

#include <QTimer>
#include <QObject>

// A class for handling all sent requests and processing responses.
// see RFC 3261 for more details.

class SIPTransactionUser;

class SIPClientTransaction : public QObject
{
  Q_OBJECT
public:
  SIPClientTransaction();

  void init(SIPTransactionUser* tu, uint32_t sessionID);

  // have we sent this kind of request
  bool waitingResponse(RequestType requestType)
  {
    return ongoingTransactionType_ == requestType
        && requestType != SIP_UNKNOWN_REQUEST;
  }

  // constructs the SIP message info struct as much as possible
  void getRequestMessageInfo(RequestType type,
                             std::shared_ptr<SIPMessageInfo> &outMessage);

  // processes incoming response. Part of our client transaction
  // returns whether we should destroy the dialog
  bool processResponse(SIPResponse& response);

  // Not implemented. Should be used to notify transaction that there was an error with response.
  void wrongResponseDestination();
  void malformedResponse();
  void responseIsError();

  // send a request
  bool startCall(QString callee);
  void endCall();
  void cancelCall();
  void registerToServer();

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

  uint32_t sessionID_;

  SIPTransactionUser* transactionUser_;
};
