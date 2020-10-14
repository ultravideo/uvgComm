#include "initiation/transaction/sipdialogmanager.h"

#include "initiation/transaction/sipdialog.h"


const uint32_t FIRSTSESSIONID = 1;


SIPDialogManager::SIPDialogManager():
  pendingConnectionMutex_(),
  nextSessionID_(FIRSTSESSIONID),
  dialogs_(),
  transactionUser_(nullptr)
{}

void SIPDialogManager::init(SIPTransactionUser *callControl)
{
  printNormal(this, "Iniating SIP Transactions");
  transactionUser_ = callControl;
}


void SIPDialogManager::uninit()
{
  dialogs_.clear();
  nextSessionID_ = FIRSTSESSIONID;
}


// reserve sessionID for a future call
uint32_t SIPDialogManager::reserveSessionID()
{
  ++nextSessionID_;
  return nextSessionID_ - 1;
}


void SIPDialogManager::startCall(SIP_URI &address, QString localAddress,
                                uint32_t sessionID, bool registered)
{
  printNormal(this, "Intializing a new dialog by sending an INVITE");
  createDialog(sessionID);
  dialogs_[sessionID]->startCall(address, localAddress, registered);
}


void SIPDialogManager::renegotiateCall(uint32_t sessionID)
{
  printDebug(DEBUG_NORMAL, "SIP Transactions", 
             "Start the process of sending a re-INVITE");

  dialogs_[sessionID]->renegotiateCall();
}


void SIPDialogManager::renegotiateAllCalls()
{
  for (auto& dialog : dialogs_) {
    renegotiateCall(dialog.first);
  }
}


uint32_t SIPDialogManager::createDialogFromINVITE(QString localAddress,
                                                 std::shared_ptr<SIPMessageInfo> &invite)
{
  Q_ASSERT(invite);

  uint32_t sessionID = reserveSessionID();
  createDialog(sessionID);

  dialogs_[sessionID]->createDialogFromINVITE(invite, localAddress);
  return sessionID;
}


void SIPDialogManager::createDialog(uint32_t sessionID)
{
  std::shared_ptr<SIPDialog> dialog = std::shared_ptr<SIPDialog> (new SIPDialog);
  dialog->init(sessionID, transactionUser_);


  QObject::connect(dialog.get(), &SIPDialog::sendRequest,
                   this, &SIPDialogManager::transportRequest);

  QObject::connect(dialog.get(), &SIPDialog::sendResponse,
                   this, &SIPDialogManager::transportResponse);

  QObject::connect(dialog.get(), &SIPDialog::dialogEnds,
                   this, &SIPDialogManager::removeDialog);

  dialogMutex_.lock();
  dialogs_[sessionID] = dialog;
  dialogMutex_.unlock();
}

void SIPDialogManager::acceptCall(uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());

  printNormal(this, "Accepting call", {"SessionID"}, {QString::number(sessionID)});

  dialogMutex_.lock();
  std::shared_ptr<SIPDialog> dialog = dialogs_[sessionID];
  dialogMutex_.unlock();

  dialog->acceptCall();
}

void SIPDialogManager::rejectCall(uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());
  dialogMutex_.lock();
  std::shared_ptr<SIPDialog> dialog = dialogs_[sessionID];
  dialogMutex_.unlock();
  dialog->rejectCall();

  removeDialog(sessionID);
}

void SIPDialogManager::endCall(uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());
  dialogMutex_.lock();
  std::shared_ptr<SIPDialog> dialog = dialogs_[sessionID];
  dialogMutex_.unlock();
  dialog->endCall();

  removeDialog(sessionID);
}

void SIPDialogManager::cancelCall(uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());

  if (dialogs_.find(sessionID) != dialogs_.end())
  {
    std::shared_ptr<SIPDialog> dialog = dialogs_[sessionID];
    dialog->cancelCall();
    removeDialog(sessionID);
  }
  else
  {
    printProgramWarning(this, "Tried to remove a non-existing dialog");
  }
}


void SIPDialogManager::endAllCalls()
{
  for(auto& dialog : dialogs_)
  {
    if(dialog.second != nullptr)
    {
      dialog.second->endCall();
    }
  }

  nextSessionID_ = FIRSTSESSIONID;
}


bool SIPDialogManager::identifySession(SIPRequest request,
                                       QString localAddress,
                                       uint32_t& out_sessionID)
{
  printNormal(this, "Starting to process identifying SIP Request dialog.");

  out_sessionID = 0;

  dialogMutex_.lock();
  for (auto i = dialogs_.begin(); i != dialogs_.end(); ++i)
  {
    if(i->second != nullptr &&
       i->second->isThisYours(request))
    {
      printNormal(this, "Found dialog matching for incoming request.");
      out_sessionID = i->first;
    }
  }
  dialogMutex_.unlock();

  // find the dialog which corresponds to the callID and tags received in request
  std::shared_ptr<SIPDialog> foundDialog;

  // we did not find existing dialog for this request
  if(out_sessionID == 0)
  {
    printNormal(this, "No existing dialog found.");

    // TODO: there is a problem if the sequence number did not match
    // and the request type is INVITE
    if(request.type == SIP_INVITE)
    {
      printNormal(this, "Someone is trying to start a SIP dialog with us!");

      out_sessionID = createDialogFromINVITE(localAddress, request.message);
      return true;
    }
    return false;
  }
  return true;
}


bool SIPDialogManager::identifySession(SIPResponse response, uint32_t& out_sessionID)
{
  printNormal(this, "Starting to process identifying SIP response dialog.");

  out_sessionID = 0;
  // find the dialog which corresponds to the callID and tags received in response
  for (auto i = dialogs_.begin(); i != dialogs_.end(); ++i)
  {
    if(i->second != nullptr &&
       i->second->isThisYours(response))
    {
      printNormal(this, "Found dialog matching the response");
      out_sessionID = i->first;
      return true;
    }
  }

  return false;
}


void SIPDialogManager::processSIPRequest(SIPRequest request, uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());
  printNormal(this, "Starting to process received SIP Request.");

  dialogMutex_.lock();

  // find the dialog which corresponds to the callID and tags received in request
  std::shared_ptr<SIPDialog> foundDialog;
  foundDialog = dialogs_[sessionID];
  dialogMutex_.unlock();

  if(!foundDialog->processRequest(request))
  {
    printNormal(this, "Ending session as a results of request.");
    removeDialog(sessionID);
  }

  printNormal(this, "Finished processing request.",
    {"SessionID"}, {QString::number(sessionID)});
}


void SIPDialogManager::processSIPResponse(SIPResponse response, uint32_t sessionID)
{
  Q_ASSERT(sessionID);
  if (sessionID == 0)
  {
    printDebug(DEBUG_PROGRAM_ERROR, this, 
               "SessionID was 0 in processSIPResponse. Should be detected earlier.");
    return;
  }

  dialogMutex_.lock();

  // find the dialog which corresponds to the callID and tags received in request
  std::shared_ptr<SIPDialog> foundDialog;
  foundDialog = dialogs_[sessionID];
  dialogMutex_.unlock();

  if (!foundDialog->processResponse(response))
  {
    removeDialog(sessionID);
  }

  printNormal(this, "Response processing finished",
      {"SessionID"}, {QString::number(sessionID)});
}


void SIPDialogManager::removeDialog(uint32_t sessionID)
{
  dialogs_.erase(dialogs_.find(sessionID));
  if (dialogs_.empty())
  {
    // TODO: This may cause problems if we have reserved a sessionID, but have not yet
    // started a new one. Maybe remove this when statistics has been updated with new
    // map for tracking sessions.
    nextSessionID_ = FIRSTSESSIONID;
  }
}
