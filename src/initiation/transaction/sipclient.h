#pragma once

#include "initiation/sipmessageprocessor.h"
#include "initiation/siptypes.h"

#include <QTimer>

// A class for handling all sent requests and processing responses. The purpose
// of this class is to keep track of request transaction state on client side.
// see RFC 3261 for more details.

class SIPTransactionUser;
class SIPDialogState;

class SIPClient : public SIPMessageProcessor
{
  Q_OBJECT
public:
  SIPClient();
  ~SIPClient();

  void setNonDialogStuff(SIP_URI& uri);

  // used to inform the other peer that this request expires. Used with INVITE
  // and REGISTER transactions
  void setNextTransactionExpires(uint32_t timeout);

  // set the internal state of client to such that we have sent a request.
  // returns whether we should actually send the message
  bool sendRequest(SIPRequestMethod type);

  // have we sent this kind of request
  // TODO: Will be obsolete with new architecture
  bool waitingResponse(SIPRequestMethod requestType)
  {
    return ongoingTransactionType_ == requestType
        && requestType != SIP_NO_REQUEST;
  }

public slots:

  // processes incoming response. Part of client transaction
  virtual void processIncomingResponse(SIPResponse& response, QVariant& content);

signals:
  // send messages to other end
  void sendNondialogRequest(SIP_URI& uri, SIPRequest& request);

  void receivedResponse(SIPRequestMethod originalRequest,
                        SIPResponseStatus status);

  void failure(QString message);

private slots:
  void requestTimeOut();

private:

  // constructs the SIP message info struct as much as possible
  void generateRequest(SIPRequestMethod type, SIPRequest &request);

  bool checkTransactionType(SIPRequestMethod transactionRequest)
  {
    return transactionRequest == ongoingTransactionType_;
  }

  // timeout is in milliseconds. Used for request timeout. The default timeout
  // is 2 seconds which should be plenty of time for RTT
  void startTimeoutTimer(int timeout = 2000)
  {
    requestTimer_.start(timeout);
  }

  void stopTimeoutTimer()
  {
    requestTimer_.stop();
  }

  void processTimeout();



  void byeTimeout();

  bool goodResponse(); // use this to filter out untimely/duplicate responses

  // used to keep track of our current transaction. This means it is used to
  // determine what request the response is responding to.
  SIPRequestMethod ongoingTransactionType_;

  QTimer requestTimer_;

  SIP_URI remoteUri_;

  std::shared_ptr<uint32_t> expires_;


};
