#pragma once

#include "siptypes.h"

#include <memory>

/* This class is responsible for maintaining the state of SIP dialog
 * and all its associated data. Use this class to check the information
 * of dialog requests. Information for responses are dealt by only the server,
 * because the server has the sent request.
 */


enum DialogState {NONACTIVE, INIATING, ACTIVE};

class SIPDialogState
{
public:
  SIPDialogState();

  // TODO: set the correct address for peer-to-peer connection
  void setPeerToPeerHostname(QString hostName)
  {
    localUri_.host = hostName;
  }

  // creates dialog which is about to start from our end
  void createNewDialog(SIP_URI remoteURI);

  void createServerDialog(SIP_URI requestURI);

  // creates the dialog from an incoming INVITE
  void createDialogFromINVITE(std::shared_ptr<SIPMessageInfo> &inMessage);

  // Generates the request message details
  void getRequestDialogInfo(SIPRequest& outRequest, QString localAddress);

  // use this to check whether incoming request belongs to this dialog
  // responses should be checked by client which sent the request
  bool correctRequestDialog(std::shared_ptr<SIPDialogInfo> dialog, RequestType type, uint32_t remoteCSeq);
  bool correctResponseDialog(std::shared_ptr<SIPDialogInfo> dialog, uint32_t messageCSeq);

  // forbid copy and assignment
  SIPDialogState(const SIPDialogState& copied) = delete;
  SIPDialogState& operator=(SIPDialogState const&) = delete;

private:

  void initLocalURI();
  void initCallInfo();

  ViaInfo getLocalVia(QString localAddress);

  // SIP Dialog fields (section 12 in RFC 3261)
  QString localTag_;
  QString remoteTag_;
  QString callID_;

  SIP_URI localUri_;
  SIP_URI remoteUri_;
  SIP_URI requestUri_;

  SIP_URI remoteContactURI_;

  // empty until first request is sent/received
  // cseq is used to determine the order of requests and must be sequential
  uint32_t localCSeq_;
  uint32_t remoteCSeq_;

  // may be empty if there is no route
  QList<SIP_URI> route_;
};
