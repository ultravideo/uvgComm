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
  ~SIPClient();

  void setDialogStuff(SIPTransactionUser* tu, uint32_t sessionID);
  void setNonDialogStuff(SIP_URI& uri);

  // Start sending of a SIP request
  bool transactionINVITE(QString callee);
  void transactionBYE();
  void transactionCANCEL();
  void transactionReINVITE(); // TODO: Remove
  void transactionREGISTER(uint32_t expires);


  // have we sent this kind of request
  bool waitingResponse(SIPRequestMethod requestType)
  {
    return ongoingTransactionType_ == requestType
        && requestType != SIP_NO_REQUEST;
  }

  // constructs the SIP message info struct as much as possible
  virtual void getRequestMessageInfo(SIPRequestMethod type,
                                     std::shared_ptr<SIPMessageHeader> &outMessage);

  // processes incoming response. Part of our client transaction
  // returns whether we should keep the dialog alive
  virtual bool processResponse(SIPResponse& response,
                               SIPDialogState& state);

  // used to record request in case we want to cancel it
  void recordRequest(SIPRequest& request)
  {
    sentRequest_ = request;
  }

  // get request we want to cancel
  SIPRequest& getRecordedRequest()
  {
    return sentRequest_;
  }

signals:
  // send messages to other end
  void sendDialogRequest(uint32_t sessionID, SIPRequestMethod type);
  void sendNondialogRequest(SIP_URI& uri, SIPRequestMethod type);

  void BYETimeout(uint32_t sessionID);

private slots:
  void requestTimeOut();

private:

  bool checkTransactionType(SIPRequestMethod transactionRequest)
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

  SIPRequestMethod getOngoingRequest() const
  {
    return ongoingTransactionType_;
  }

  // set the internal state of client to such that we have sent a request.
  // returns whether we should actually send the message
  virtual bool startTransaction(SIPRequestMethod type);

  virtual void byeTimeout();

  bool goodResponse(); // use this to filter out untimely/duplicate responses

  // used to keep track of our current transaction. This means it is used to
  // determine what request the response is responding to.
  SIPRequestMethod ongoingTransactionType_;
  SIPRequest sentRequest_;

  QTimer requestTimer_;

  uint32_t sessionID_;

  SIPTransactionUser* transactionUser_;

  SIP_URI remoteUri_;

  uint32_t expires_;
};
