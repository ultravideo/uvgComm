#pragma once

#include "siptypes.h"

/* This class implements the behavior defined in RFC3261 for component
 * User Agent Server (UAS). See section 8.2 for details.
 */


class SIPTransactionUser;

class SIPServerTransaction : public QObject
{
   Q_OBJECT
public:
  SIPServerTransaction();

  void init(SIPTransactionUser* tu, uint32_t sessionID);

  // processes incoming request. Part of our server transaction
  void processRequest(SIPRequest& request);
  void wrongRequestDestination();
  void malformedRequest();

  void getResponseMessage(std::shared_ptr<SIPMessageInfo> &outMessage,
                          ResponseType type);

  void acceptCall();
  void rejectCall();

signals:

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
