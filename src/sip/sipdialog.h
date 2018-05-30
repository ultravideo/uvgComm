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

  void init(SIP_URI remoteURI);

  // creates dialog which is about to start from our end
  void createDialog(QString hostName);

  // creates the dialog from an incoming INVITE
  void processFirstINVITE(std::shared_ptr<SIPMessageInfo> &inMessage);

  // Generates the request message details
  void getRequestDialogInfo(RequestType type, QString localAddress, std::shared_ptr<SIPMessageInfo> &outMessage);

  // use this to check whether incoming request belongs to this dialog
  // responses should be checked by client which sent the request
  bool correctRequestDialog(std::shared_ptr<SIPDialogInfo> dialog, RequestType type, uint32_t remoteCSeq);

  // forbid copy and assignment
  SIPDialog(const SIPDialog& copied) = delete;
  SIPDialog& operator=(SIPDialog const&) = delete;

private:

  // TODO: This same function also exists in sipregistration
  void initLocalURI();
  ViaInfo getLocalVia(QString localAddress);

  // SIP Dialog fields (section 12 in RFC 3261)
  QString localTag_;
  QString remoteTag_;
  QString callID_;

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
