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

  // send a accept/reject response to received request according to user.
  void respondOK();
  void respondDECLINE();

  bool isCANCELYours(std::shared_ptr<SIPMessageHeader> cancel);

signals:

  // a signal that response of the following type should be sent.
  void sendResponse(uint32_t sessionID, SIPResponse& response);

private:

  void responseSender(SIPResponseStatus type);

  // Copies the fields of to a response which are direct copies of the request.
  // includes at least via, to, from, CallID and cseq
  void copyMessageDetails(std::shared_ptr<SIPMessageHeader> &inMessage,
                          std::shared_ptr<SIPMessageHeader> &copy);

  uint32_t sessionID_;

  // used for copying data to response
  std::shared_ptr<SIPMessageHeader> receivedRequest_;

  SIPTransactionUser* transactionUser_;
};
