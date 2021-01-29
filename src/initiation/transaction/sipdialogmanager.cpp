#include "initiation/transaction/sipdialogmanager.h"


#include <QVariant>

const uint32_t FIRSTSESSIONID = 1;

// 1 minute for the user to react
const unsigned int INVITE_TIMEOUT = 60000;

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


void SIPDialogManager::startCall(NameAddr &address, QString localAddress,
                                uint32_t sessionID, bool registered)
{
  printNormal(this, "Intializing a new dialog by sending an INVITE");
  createDialog(sessionID);

  dialogs_[sessionID]->state.createNewDialog(address);

  // in peer-to-peer calls we use the actual network address as local URI
  if (!registered)
  {
    dialogs_[sessionID]->state.setLocalHost(localAddress);
  }

  // this start call will commence once the connection has been established
  if(!dialogs_[sessionID]->client.transactionINVITE(address.realname, INVITE_TIMEOUT))
  {
    printWarning(this, "Could not start a call according to client.");
  }
}


void SIPDialogManager::renegotiateCall(uint32_t sessionID)
{
  printDebug(DEBUG_NORMAL, "SIP Transactions", 
             "Start the process of sending a re-INVITE");

  dialogs_[sessionID]->client.transactionReINVITE();
}


void SIPDialogManager::renegotiateAllCalls()
{
  for (auto& dialog : dialogs_)
  {
    renegotiateCall(dialog.first);
  }
}


uint32_t SIPDialogManager::createDialogFromINVITE(QString localAddress,
                                                 SIPRequest &invite)
{
  uint32_t sessionID = reserveSessionID();
  createDialog(sessionID);

  QVariant content;
  dialogs_[sessionID]->state.processIncomingRequest(invite, content);
  dialogs_[sessionID]->state.setLocalHost(localAddress);

  return sessionID;
}


void SIPDialogManager::createDialog(uint32_t sessionID)
{
  std::shared_ptr<SIPDialog> dialog = std::shared_ptr<SIPDialog> (new SIPDialog);

  dialogMutex_.lock();
  dialogs_[sessionID] = dialog;
  dialogMutex_.unlock();

  dialogs_[sessionID]->client.setDialogStuff(transactionUser_, sessionID);
  dialogs_[sessionID]->server.init(transactionUser_, sessionID);

  QObject::connect(&dialogs_[sessionID]->client, &SIPClient::sendDialogRequest,
                   this, &SIPDialogManager::processOutgoingRequest);

  QObject::connect(&dialogs_[sessionID]->client, &SIPClient::BYETimeout,
                   this, &SIPDialogManager::removeDialog);

  QObject::connect(&dialogs_[sessionID]->server, &SIPServer::sendResponse,
                   this, &SIPDialogManager::transportResponse);
}


void SIPDialogManager::acceptCall(uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());

  printNormal(this, "Accepting call", {"SessionID"}, {QString::number(sessionID)});

  dialogMutex_.lock();
  std::shared_ptr<SIPDialog> dialog = dialogs_[sessionID];
  dialogMutex_.unlock();

  dialog->server.respondOK();
}

void SIPDialogManager::rejectCall(uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());
  dialogMutex_.lock();
  std::shared_ptr<SIPDialog> dialog = dialogs_[sessionID];
  dialogMutex_.unlock();
  dialog->server.respondDECLINE();

  removeDialog(sessionID);
}

void SIPDialogManager::endCall(uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());
  dialogMutex_.lock();
  std::shared_ptr<SIPDialog> dialog = dialogs_[sessionID];
  dialogMutex_.unlock();
  dialog->client.transactionBYE();

  removeDialog(sessionID);
}

void SIPDialogManager::cancelCall(uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());

  if (dialogs_.find(sessionID) != dialogs_.end())
  {
    std::shared_ptr<SIPDialog> dialog = dialogs_[sessionID];
    dialog->client.transactionCANCEL();
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
      dialog.second->client.transactionBYE();
    }
  }

  nextSessionID_ = FIRSTSESSIONID;
}


bool SIPDialogManager::identifySession(SIPRequest& request,
                                       QString localAddress,
                                       uint32_t& out_sessionID)
{
  printNormal(this, "Starting to process identifying SIP Request dialog.");

  out_sessionID = 0;

  dialogMutex_.lock();
  for (auto i = dialogs_.begin(); i != dialogs_.end(); ++i)
  {
    if(i->second != nullptr)
    {
      if ((request.method == SIP_CANCEL && i->second->server.isCANCELYours(request)) ||
          i->second->state.correctRequestDialog(request.message, request.method,
                                                request.message->cSeq.cSeq))
      {
        printNormal(this, "Found dialog matching for incoming request.");
        out_sessionID = i->first;
      }
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
    if(request.method == SIP_INVITE)
    {
      printNormal(this, "Someone is trying to start a SIP dialog with us!");

      out_sessionID = createDialogFromINVITE(localAddress, request);
      return true;
    }
    return false;
  }
  return true;
}


bool SIPDialogManager::identifySession(SIPResponse &response, uint32_t& out_sessionID)
{
  printNormal(this, "Starting to process identifying SIP response dialog.");

  out_sessionID = 0;
  // find the dialog which corresponds to the callID and tags received in response
  for (auto i = dialogs_.begin(); i != dialogs_.end(); ++i)
  {
    if(i->second != nullptr &&
       i->second->state.correctResponseDialog(response.message, response.message->cSeq.cSeq) &&
       i->second->client.waitingResponse(response.message->cSeq.method))
    {
      printNormal(this, "Found dialog matching the response");
      out_sessionID = i->first;
      return true;
    }
  }

  return false;
}


void SIPDialogManager::processSIPRequest(SIPRequest &request, uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());
  printNormal(this, "Starting to process received SIP Request.");

  dialogMutex_.lock();

  // find the dialog which corresponds to the callID and tags received in request
  std::shared_ptr<SIPDialog> foundDialog;
  foundDialog = dialogs_[sessionID];
  dialogMutex_.unlock();

  QVariant content; // unused
  foundDialog->server.processIncomingRequest(request, content);
  if(!foundDialog->server.shouldBeKeptAlive())
  {
    printNormal(this, "Ending session as a results of request.");
    removeDialog(sessionID);
  }

  printNormal(this, "Finished processing request.",
    {"SessionID"}, {QString::number(sessionID)});
}


void SIPDialogManager::processSIPResponse(SIPResponse &response, uint32_t sessionID)
{
  Q_ASSERT(sessionID);
  if (sessionID == 0)
  {
    printDebug(DEBUG_PROGRAM_ERROR, this, 
               "SessionID was 0 in processSIPResponse. Should be detected earlier.");
    return;
  }
  if (response.message == nullptr)
  {
    printProgramError(this, "SIPDialog got a message without header");
    return;
  }

  dialogMutex_.lock();

  // find the dialog which corresponds to the callID and tags received in request
  std::shared_ptr<SIPDialog> foundDialog;
  foundDialog = dialogs_[sessionID];
  dialogMutex_.unlock();

  QVariant content; // unused

  foundDialog->state.processIncomingResponse(response, content);
  foundDialog->client.processIncomingResponse(response, content);

  if (!foundDialog->client.shouldBeKeptAlive())
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


void SIPDialogManager::processOutgoingRequest(uint32_t sessionID, SIPRequest& request)
{
  printNormal(this, "Initiate sending of a dialog request");

  dialogMutex_.lock();
  std::shared_ptr<SIPDialog> foundDialog = dialogs_[sessionID];
  dialogMutex_.unlock();

  QVariant content; // unused
  foundDialog->state.processOutgoingRequest(request, content);

  emit transportRequest(sessionID, request);
  printNormal(this, "Finished sending of a dialog request");
}
