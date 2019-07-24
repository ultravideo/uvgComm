#pragma once

#include "initiation/siptypes.h"

#include <QTimer>
#include <QObject>

// A class for handling all sent requests and processing responses. The purpose
// of this class is to keep track of request transaction state on client side.
// see RFC 3261 for more details.

class SIPTransactionUser;
class SIPDialogState;

class SIPClientTransaction : public QObject
{
  Q_OBJECT
public:
  SIPClientTransaction(SIPTransactionUser* tu);

  void init();

  // have we sent this kind of request
  bool waitingResponse(RequestType requestType)
  {
    return ongoingTransactionType_ == requestType
        && requestType != SIP_NO_REQUEST;
  }

  // constructs the SIP message info struct as much as possible
  void getRequestMessageInfo(RequestType type,
                             std::shared_ptr<SIPMessageInfo> &outMessage);

  // processes incoming response. Part of our client transaction
  // returns whether we should destroy the dialog
  virtual bool processResponse(SIPResponse& response,
                               std::shared_ptr<SIPDialogState> state) = 0;

  // Not implemented. Should be used to notify transaction that there was an error with response.
  void wrongResponseDestination();
  void malformedResponse();
  void responseIsError();

  void startTimer(RequestType type);

protected:

  // timeout is in milliseconds. Used for request timeout
  void startTimeoutTimer(int timeout = 2000)
  {
    requestTimer_.start(timeout);
  }

  void stopTimeoutTimer()
  {
    requestTimer_.stop();
  }

  SIPTransactionUser* getTransactionUser()
  {
    return transactionUser_;
  }

  virtual void processTimeout();

  RequestType getOngoingRequest()
  {
    return ongoingTransactionType_;
  }

  virtual void sendRequest(RequestType type) = 0;

private slots:
  void requestTimeOut();

private:
  bool goodResponse(); // use this to filter out untimely/duplicate responses

  // used to determine what type of request the response is for
  RequestType ongoingTransactionType_;

  // TODO: Probably need to record the whole message and check that the details are ok.

  QTimer requestTimer_;


  SIPTransactionUser* transactionUser_;
};
