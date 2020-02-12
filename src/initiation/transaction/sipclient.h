#pragma once

#include "initiation/siptypes.h"

#include <QTimer>
#include <QObject>

// A class for handling all sent requests and processing responses. The purpose
// of this class is to keep track of request transaction state on client side.
// see RFC 3261 for more details.

class SIPTransactionUser;
class SIPDialogState;

class SIPClient : public QObject
{
  Q_OBJECT
public:
  SIPClient();

  // have we sent this kind of request
  bool waitingResponse(RequestType requestType)
  {
    return ongoingTransactionType_ == requestType
        && requestType != SIP_NO_REQUEST;
  }

  // constructs the SIP message info struct as much as possible
  virtual void getRequestMessageInfo(RequestType type,
                                     std::shared_ptr<SIPMessageInfo> &outMessage);

  // processes incoming response. Part of our client transaction
  // returns whether we should keep the dialog alive
  virtual bool processResponse(SIPResponse& response,
                               SIPDialogState& state) = 0;

  // Not implemented. Should be used to notify transaction that there was an error with response.
  void wrongResponseDestination();
  void malformedResponse();
  void responseIsError();

protected:

  bool checkTransactionType(RequestType transactionRequest)
  {
    return transactionRequest == ongoingTransactionType_;
  }

  // timeout is in milliseconds. Used for request timeout
  void startTimeoutTimer(int timeout = 2000)
  {
    requestTimer_.start(timeout);
  }

  void stopTimeoutTimer()
  {
    requestTimer_.stop();
  }

  virtual void processTimeout();

  RequestType getOngoingRequest() const
  {
    return ongoingTransactionType_;
  }

  // set the internal state of client to such that we have sent a request.
  // returns whether we should actually send the message
  virtual bool startTransaction(RequestType type);

private slots:
  void requestTimeOut();

private:
  bool goodResponse(); // use this to filter out untimely/duplicate responses

  // used to keep track of our current transaction. This means it is used to
  // determine what request the response is responding to.
  RequestType ongoingTransactionType_;

  QTimer requestTimer_;
};
