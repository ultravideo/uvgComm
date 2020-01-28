#pragma once

#include "initiation/siptypes.h"

#include <memory>

/* This class is responsible for maintaining the state of SIP dialog
 * and all its associated data. Use this class to check the information
 * of dialog requests. Information for responses are dealt by only the server,
 * because the server has the sent request.
 */


class SIPDialogState
{
public:
  SIPDialogState();

  // creates dialog which is about to start from our end
  // needs local address in case we have not registered
  // and this is a peer-to-peer dialog
  void createNewDialog(SIP_URI remoteURI, QString localAddress, bool registered);

  // create a connection to server to be used for sending the REGISTER request
  // does not actually create dialog.
  void createServerConnection(SIP_URI requestURI);

  // creates the dialog from an incoming INVITE
  void createDialogFromINVITE(std::shared_ptr<SIPMessageInfo> &inMessage,
                              QString hostName);

  // Generates the request message details
  void getRequestDialogInfo(SIPRequest& outRequest);

  void setRequestUri(SIP_URI& remoteContact)
  {
    requestUri_ = remoteContact;
  }

  void setRoute(QList<SIP_URI>& route);

  // use this to check whether incoming request belongs to this dialog
  // responses should be checked by client which sent the request
  bool correctRequestDialog(std::shared_ptr<SIPDialogInfo> dialog,
                            RequestType type, uint32_t remoteCSeq);
  bool correctResponseDialog(std::shared_ptr<SIPDialogInfo> dialog,
                             uint32_t messageCSeq);


  // set and get session activity state
  bool getState() const
  {
    return sessionState_;
  }

  void setState(bool state)
  {
    sessionState_ = state;
  }

  // forbid copy and assignment
  SIPDialogState(const SIPDialogState& copied) = delete;
  SIPDialogState& operator=(SIPDialogState const&) = delete;

private:

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
  SIP_URI localURI_; // local address-of-record.
  SIP_URI remoteURI_; // remote address-of-record.

  SIP_URI requestUri_;

  // empty until first request is sent/received
  // cseq is used to determine the order of requests and must be sequential
  uint32_t localCSeq_;
  uint32_t remoteCSeq_;

  // may be empty if there is no route
  QList<SIP_URI> route_;

  bool sessionState_;
};
