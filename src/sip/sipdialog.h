#pragma once

#include "siptypes.h"

#include <memory>

/* This class is responsible for maintaining the state of SIP dialog
 * and all its associated data. Use this class to check the information
 * of dialog requests. Information for responses are dealt by only the server,
 * because the server has the sent request.
 */


enum DialogState {NONACTIVE, INIATING, ACTIVE};

class SIPDialog
{
public:
  SIPDialog();

  // creates dialog which is about to start from our end
  void initDialog(QString serverName, SIP_URI localUri, SIP_URI remoteUri);

  // creates the dialog from an incoming INVITE
  void processINVITE(std::shared_ptr<SIPDialogInfo> dialog, uint32_t cSeq,
                     SIP_URI localUri, SIP_URI remoteUri);

  // this will fill the missing dialog details from message
  void getRequestDialogInfo(RequestType type, std::shared_ptr<SIPMessageInfo> message);

  // generate a random callID
  void getRegisterDialogInfo(RequestType type, std::shared_ptr<SIPMessageInfo> message);

  // use this to check whether incoming request belongs to this dialog
  // responses should be checked by client which sent the request
  bool correctRequestDialog(std::shared_ptr<SIPDialogInfo> dialog, RequestType type, uint32_t remoteCSeq);

  // forbid copy and assignment
  SIPDialog(const SIPDialog& copied) = delete;
  SIPDialog& operator=(SIPDialog const&) = delete;

private:

  // SIP Dialog fields (section 12 in RFC 3261)
  QString localTag_;
  QString remoteTag_;
  QString callID_;

  // TODO: incorporate URIs to this class
  SIP_URI localUri_;
  SIP_URI remoteUri_;

  // empty until first request is sent/received
  // cseq is used to determine the order of requests and must be sequential
  uint32_t localCSeq_;
  uint32_t remoteCSeq_;

  // may be empty if there is no route
  QList<SIP_URI> route_;

  bool secure_;
};
