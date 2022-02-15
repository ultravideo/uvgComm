#pragma once

#include "initiation/sipmessageprocessor.h"
#include "initiation/siptypes.h"

#include <QTimer>

/* A class for handling all sent requests and processing responses. The purpose
 * of this class is to keep track of request transaction state on client side.
 * See RFC 3261 for more details. */

class SIPClient : public SIPMessageProcessor
{
  Q_OBJECT
public:
  SIPClient();
  ~SIPClient();

  void sendREGISTER(uint32_t timeout);
  void sendINVITE(uint32_t timeout);
  void sendBYE();
  void sendCANCEL();

  bool registrationActive()
  {
    return activeRegistration_;
  }

  bool shouldBeDestroyed()
  {
    return !shouldLive_;
  }

public slots:

  // processes incoming response. Part of client transaction
  virtual void processIncomingResponse(SIPResponse& response, QVariant& content,
                                       bool retryRequest);

  void refreshRegistration();

private slots:
  void requestTimeOut();

private:

  bool correctRequestType(SIPRequestMethod method);

  bool sendRequest(SIPRequestMethod method);
  bool sendRequest(SIPRequestMethod method,
                   uint32_t expires);

  // constructs the SIP message info struct as much as possible
  SIPRequest generateRequest(SIPRequestMethod method);

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

  uint32_t expires_;

  // This variable is used to avoid HEAP corruption by destroying the flow afterwards
  // instead of accidentally delete ourselves with signals etc.
  bool shouldLive_;

  bool activeRegistration_;
};
