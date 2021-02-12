#pragma once

#include "initiation/sipmessageprocessor.h"
#include "initiation/siptypes.h"

#include <memory>

/* This class is responsible for maintaining the state of SIP dialog
 * and all its associated data. Use this class to check the information
 * of dialog requests. Information for responses are dealt by only the server,
 * because the server has the sent request.
 */


/* Responsible for the following fields:
 * - Call-ID
 * - To
 * - From
 * - Route
 *
 * Also responsible for the  Request-URI fields and CSeq number.
 */

class SIPDialogState : public SIPMessageProcessor
{
  Q_OBJECT
public:
  SIPDialogState();
  ~SIPDialogState();

  // sets the from and to addresses of this dialog as well as the request-URI
  void init(NameAddr& local, NameAddr& remote, bool createDialog);

  // create a connection to server to be used for sending the REGISTER request
  // does not actually create dialog.
  void createServerConnection(NameAddr& local, SIP_URI requestURI);


  // Use this to check whether incoming request belongs to this dialog.
  // Please use the header field values here so the messages is checked correctly.
  bool correctRequestDialog(QString callID, QString toTag, QString fromTag);
  bool correctResponseDialog(QString callID, QString toTag, QString fromTag);


public slots:

  // Adds dialog info to request
  virtual void processOutgoingRequest(SIPRequest& request, QVariant& content);

  // Creates a dialog if it is INVITE
  virtual void processIncomingRequest(SIPRequest& request, QVariant& content);

  // Gets the correct requestURI from INVITE OK contact
  virtual void processIncomingResponse(SIPResponse& response, QVariant& content);


private:

  // forbid copy and assignment
  SIPDialogState(const SIPDialogState& copied) = delete;
  SIPDialogState& operator=(SIPDialogState const&) = delete;

  // set our information as well as generate callID and tags
  void initDialog();

  // set our information, callID and tags
  // generate our own tag if needed.
  void setDialog(QString callID);

  uint32_t initialCSeqNumber();

  // SIP Dialog fields (section 12 in RFC 3261)
  QString localTag_;
  QString remoteTag_;
  QString callID_;

  // address-of-record is the SIP address if one exists. If we have not registered to
  // server we use our local IP address.
  NameAddr localURI_; // local address-of-record.
  NameAddr remoteURI_; // remote address-of-record.

  SIP_URI requestUri_;

  // empty until first request is sent/received
  // cseq is used to determine the order of requests and must be sequential

  // both our peer and us have separate cSeq numbers
  uint32_t localCseq_;
  uint32_t remoteCseq_;

  // may be empty if there is no route
  QList<SIPRouteLocation> route_;

  SIPRequest previousRequest_;
};
