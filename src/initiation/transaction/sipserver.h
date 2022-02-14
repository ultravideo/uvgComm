#pragma once

#include "initiation/sipmessageprocessor.h"
#include "initiation/siptypes.h"

class SIPTransactionUser;

/* This class implements the behavior defined in RFC3261 for component
 * User Agent Server (UAS). See section 8.2 for details.
 *
 * Server handles processing received requests and sending correct responses.
 */

class SIPServer : public SIPMessageProcessor
{
   Q_OBJECT
public:
  SIPServer();

  void respond_INVITE_RINGING();
  void respond_INVITE_OK();
  void respond_INVITE_DECLINE();

  bool doesCANCELMatchRequest(SIPRequest& request) const;

  bool shouldBeDestroyed()
  {
    return !shouldLive_;
  }

public slots:

    // processes incoming request. Part of server transaction
  virtual void processIncomingRequest(SIPRequest& request, QVariant& content,
                                      SIPResponseStatus generatedResponse);


private:

  bool isCANCELYours(SIPRequest &cancel);

  void createResponse(SIPResponseStatus status);

  // Copies the fields of to a response which are direct copies of the request.
  // includes at least via, to, from, CallID and cseq
  void copyResponseDetails(std::shared_ptr<SIPMessageHeader> &inMessage,
                          std::shared_ptr<SIPMessageHeader> &copy);

  bool equalURIs(SIP_URI& first, SIP_URI& second);

  bool equalToFrom(ToFrom& first, ToFrom& second);

  bool shouldLive_;

  // used for copying data to response
  std::shared_ptr<SIPRequest> receivedRequest_;
};
