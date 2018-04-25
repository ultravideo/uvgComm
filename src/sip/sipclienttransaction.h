#pragma once

#include "siptypes.h"

#include <QTimer>
#include <QObject>

class SIPTransactionUser;

enum CallState {INACTIVE, NEGOTIATING, RUNNNING, ENDING};

class SIPClientTransaction : public QObject
{
  Q_OBJECT
public:
  SIPClientTransaction();

  void init(SIPTransactionUser* tu, uint32_t sessionID);
  bool ourResponse(SIPResponse& response)
  {
    return false;
  }

  SIPResponse& getResponseData(ResponseType type)
  {
    SIPResponse r;
    return r;
  }

  //processes incoming response. Part of our client transaction
  void processResponse(SIPResponse& response);
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

  // used to check if the received response is for our request
  RequestType ongoingTransactionType_;

  QTimer requestTimer_;
  bool connected_;

  uint32_t sessionID_;

  CallState state_;

  // waiting to be sent once the connecion has been opened
  RequestType pendingRequest_;

  SIPTransactionUser* transactionUser_;
};
