#pragma once

#include "initiation/siptypes.h"

/* This class implements the behavior defined in RFC3261 for component
 * User Agent Server (UAS). See section 8.2 for details.
 *
 * Server transaction handles processing received requests and sending correct responses.
 */


class SIPTransactionUser;

class SIPServerTransaction : public QObject
{
   Q_OBJECT
public:
  SIPServerTransaction();

  void init(SIPTransactionUser* tu, uint32_t sessionID);

  // processes incoming request. Part of our server transaction
  // returns whether we should continue this session
  bool processRequest(SIPRequest& request);

  // inform the transaction that we have received a faulty request.
  void wrongRequestDestination();
  void malformedRequest();

  void getResponseMessage(std::shared_ptr<SIPMessageInfo> &outMessage,
                          ResponseType type);

  // set the request details so we can use them when sending response
  // used in special cases where we know the response before calling process function.
  void setCurrentRequest(SIPRequest& request);

  // send a accept/reject response to received request according to user.
  void acceptCall();
  void rejectCall();

signals:

  // a signal that response of the following type should be sent.
  void sendResponse(uint32_t sessionID, ResponseType type, RequestType originalRequest);

private:

  void responseSender(ResponseType type, bool finalResponse);
  bool goodRequest(); // use this to filter out untimely/duplicate requests

  // copies the necessary details from
  void copyMessageDetails(std::shared_ptr<SIPMessageInfo> &inMessage,
                          std::shared_ptr<SIPMessageInfo> &copy);


  uint32_t sessionID_;

  // used for copying data to response
  std::shared_ptr<SIPMessageInfo> receivedRequest_;

  SIPTransactionUser* transactionUser_;
};
