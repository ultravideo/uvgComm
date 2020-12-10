#pragma once

#include <QObject>

#include "initiation/siptypes.h"

/* This class implements the behavior defined in RFC3261 for component
 * User Agent Server (UAS). See section 8.2 for details.
 *
 * Server handles processing received requests and sending correct responses.
 */


class SIPTransactionUser;
class SIPDialogState;


class SIPServer : public QObject
{
   Q_OBJECT
public:
  SIPServer();

  void init(SIPTransactionUser* tu, uint32_t sessionID);

  // processes incoming request. Part of our server transaction
  // returns whether we should continue this session
  bool processRequest(SIPRequest& request,
                      SIPDialogState& state);

  void getResponseMessage(std::shared_ptr<SIPMessageBody> &outMessage,
                          SIPResponseStatus type);

  // set the request details so we can use them when sending response
  // used in special cases where we know the response before calling process function.
  void setCurrentRequest(SIPRequest& request);

  // send a accept/reject response to received request according to user.
  void responseAccept();
  void respondReject();

  bool isCancelYours(std::shared_ptr<SIPMessageBody> cancel);

signals:

  // a signal that response of the following type should be sent.
  void sendResponse(uint32_t sessionID, SIPResponseStatus type);

private:

  void responseSender(SIPResponseStatus type);
  bool goodRequest(); // use this to filter out untimely/duplicate requests

  // Copies the fields of to a response which are direct copies of the request.
  // includes at least via, to, from, CallID and cseq
  void copyMessageDetails(std::shared_ptr<SIPMessageBody> &inMessage,
                          std::shared_ptr<SIPMessageBody> &copy);


  uint32_t sessionID_;

  // used for copying data to response
  std::shared_ptr<SIPMessageBody> receivedRequest_;

  SIPTransactionUser* transactionUser_;
};
