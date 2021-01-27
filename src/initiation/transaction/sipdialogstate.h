#pragma once

#include "initiation/sipmessageprocessor.h"
#include "initiation/siptypes.h"

#include <memory>

/* This class is responsible for maintaining the state of SIP dialog
 * and all its associated data. Use this class to check the information
 * of dialog requests. Information for responses are dealt by only the server,
 * because the server has the sent request.
 */


class SIPDialogState : public SIPMessageProcessor
{
public:
  SIPDialogState();
  ~SIPDialogState();

  // if this is a peer-to-peer call so we don't use the server address in config
  void setLocalHost(QString localAddress);

  // creates dialog which is about to start from our end
  // needs local address in case we have not registered
  // and this is a peer-to-peer dialog
  void createNewDialog(NameAddr& remote);

  // create a connection to server to be used for sending the REGISTER request
  // does not actually create dialog.
  void createServerConnection(SIP_URI requestURI);


  // Adds dialog info to request
  virtual void processOutgoingRequest(SIPRequest& request, QVariant& content);

  // Creates a dialog if it is INVITE
  virtual void processIncomingRequest(SIPRequest& request, QVariant& content);

  // Gets the correct requestURI from INVITE OK contact
  virtual void processIncomingResponse(SIPResponse& response, QVariant& content);


  // use this to check whether incoming request belongs to this dialog
  // responses should be checked by client which sent the request
  bool correctRequestDialog(std::shared_ptr<SIPMessageHeader> &inMessage,
                            SIPRequestMethod type, uint32_t remoteCSeq);
  bool correctResponseDialog(std::shared_ptr<SIPMessageHeader> &inMessage,
                             uint32_t messageCSeq, bool recordToTag = true);


  // set and get whether the dialog is active
  bool getState() const
  {
    return sessionState_;
  }

  void setState(bool state)
  {
    sessionState_ = state;
  }

private:

  // forbid copy and assignment
  SIPDialogState(const SIPDialogState& copied) = delete;
  SIPDialogState& operator=(SIPDialogState const&) = delete;

  // set our information as well as generate callID and tags
  void initDialog();
  // set our information, callID and tags
  // generate our own tag if needed.
  void setDialog(QString callID);

  void initLocalURI();

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
  uint32_t localCSeq_;
  uint32_t remoteCSeq_;

  // may be empty if there is no route
  QList<SIPRouteLocation> route_;

  bool sessionState_;
};
