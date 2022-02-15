#pragma once

#include "initiation/sipmessageprocessor.h"
#include "initiation/siptypes.h"

#include <QTimer>

/* A class for handling all sent requests and processing responses. The purpose
 * of this class is to keep track of request transaction state on client side.
 * See RFC 3261 for more details. */

class SIPTransactionUser;

class SIPClient : public SIPMessageProcessor
{
  Q_OBJECT
public:
  SIPClient();
  ~SIPClient();

  // Used to inform the other peer when this request expires. Used with INVITE
  // and REGISTER transactions
  void setNextTransactionExpires(uint32_t timeout);

  void sendREGISTER(uint32_t timeout);

  bool registrationActive()
  {
    return activeRegistration_;
  }

public slots:

  virtual void processOutgoingRequest(SIPRequest& request, QVariant& content);

  // processes incoming response. Part of client transaction
  virtual void processIncomingResponse(SIPResponse& response, QVariant& content,
                                       bool retryRequest);

  void refreshRegistration();


signals:

  void failure(QString message);

private slots:
  void requestTimeOut();

private:

  void sendREGISTERRequest(uint32_t expires);

  // constructs the SIP message info struct as much as possible
  void generateRequest(SIPRequest &request);

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

  std::shared_ptr<uint32_t> expires_;

  bool activeRegistration_;
};
