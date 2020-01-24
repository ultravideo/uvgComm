#include "initiation/transaction/siptransactions.h"

#include "initiation/transaction/sipdialogstate.h"
#include "initiation/transport/siptransport.h"

#include "initiation/transaction/sipdialogclient.h"
#include "initiation/transaction/sipservertransaction.h"


const uint32_t FIRSTSESSIONID = 1;


SIPTransactions::SIPTransactions():
  pendingConnectionMutex_(),
  nextSessionID_(FIRSTSESSIONID),
  dialogs_(),
  directContactAddresses_(),
  transactionUser_(nullptr)
{}

void SIPTransactions::init(SIPTransactionUser *callControl)
{
  qDebug() << "Iniating SIP Transactions";
  transactionUser_ = callControl;
}


void SIPTransactions::uninit()
{
  //TODO: delete all dialogs
  for(auto i = dialogs_.begin(); i != dialogs_.end(); ++i)
  {
    destroyDialog(i->first);
  }

  dialogs_.clear();
  nextSessionID_ = FIRSTSESSIONID;
}


// reserve sessionID for a future call
uint32_t SIPTransactions::reserveSessionID()
{
  ++nextSessionID_;
  return nextSessionID_ - 1;
}


void SIPTransactions::registerTask()
{
  qDebug() << "Registering us to a new SIP server with REGISTER task";
}


void SIPTransactions::startCall(Contact &address, QHostAddress localAddress,
                                uint32_t sessionID, bool registered)
{
  qDebug() << "SIP," << metaObject()->className() << ": Intializing a new dialog with INVITE";

  std::shared_ptr<SIPDialogData> dialogData;
  createBaseDialog(sessionID, dialogData);
  dialogData->state->createNewDialog(SIP_URI{TRANSPORTTYPE, address.username, address.username,
                                             address.remoteAddress,  0, {}},
                                     localAddress.toString(), registered);

  // this start call will commence once the connection has been established
  if(!dialogData->client->startCall(address.realName))
  {
    printDebug(DEBUG_WARNING, this, DC_START_CALL,
               "Could not start a call according to session.");
  }
}


void SIPTransactions::renegotiateCall(uint32_t sessionID)
{
  printDebug(DEBUG_NORMAL, "SIP Transactions", DC_NEGOTIATING,
             "Start the process of sending a re-INVITE");

  dialogs_[sessionID]->client->renegotiateCall();
}


void SIPTransactions::renegotiateAllCalls()
{
  for (auto& dialog : dialogs_) {
    renegotiateCall(dialog.first);
  }
}


uint32_t SIPTransactions::createDialogFromINVITE(QHostAddress localAddress,
                                                 std::shared_ptr<SIPMessageInfo> &invite)
{
  Q_ASSERT(invite);

  uint32_t sessionID = reserveSessionID();

  std::shared_ptr<SIPDialogData> dialog;
  createBaseDialog(sessionID, dialog);

  dialog->state->createDialogFromINVITE(invite, localAddress.toString());
  return sessionID;
}


void SIPTransactions::createBaseDialog(uint32_t sessionID,
                                       std::shared_ptr<SIPDialogData>& dialog)
{
  dialog = std::shared_ptr<SIPDialogData> (new SIPDialogData);
  dialogMutex_.lock();

  dialog->state = std::shared_ptr<SIPDialogState> (new SIPDialogState());

  dialog->client = std::shared_ptr<SIPDialogClient> (new SIPDialogClient(transactionUser_));
  dialog->client->init();
  dialog->client->setSessionID(sessionID);

  dialog->server = std::shared_ptr<SIPServerTransaction> (new SIPServerTransaction);
  dialog->server->init(transactionUser_, sessionID);

  dialogs_[sessionID] = dialog;

  dialogMutex_.unlock();

  QObject::connect(dialog->client.get(), &SIPDialogClient::sendDialogRequest,
                   this, &SIPTransactions::sendDialogRequest);

  QObject::connect(dialog->server.get(), &SIPServerTransaction::sendResponse,
                   this, &SIPTransactions::sendResponse);
}

void SIPTransactions::acceptCall(uint32_t sessionID, QString contactAddress,
                                 uint16_t contactPort)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());

  qDebug() << "Accept, SIPTransaction" << "accepting call:" << sessionID;

  dialogMutex_.lock();
  std::shared_ptr<SIPDialogData> dialog = dialogs_[sessionID];
  dialogMutex_.unlock();

  dialog->state->setOurContactAddress(contactAddress, contactPort);
  dialog->server->acceptCall();
}

void SIPTransactions::rejectCall(uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());
  dialogMutex_.lock();
  std::shared_ptr<SIPDialogData> dialog = dialogs_[sessionID];
  dialogMutex_.unlock();
  dialog->server->rejectCall();

  destroyDialog(sessionID);
  removeDialog(sessionID);
}

void SIPTransactions::endCall(uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());
  dialogMutex_.lock();
  std::shared_ptr<SIPDialogData> dialog = dialogs_[sessionID];
  dialogMutex_.unlock();
  dialog->client->endCall();

  destroyDialog(sessionID);
  removeDialog(sessionID);
}

void SIPTransactions::cancelCall(uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());

  std::shared_ptr<SIPDialogData> dialog = dialogs_[sessionID];
  dialog->client->cancelCall();

  destroyDialog(sessionID);
  removeDialog(sessionID);
}


void SIPTransactions::endAllCalls()
{
  for(auto& dialog : dialogs_)
  {
    if(dialog.second != nullptr)
    {
      dialog.second->client->endCall();
    }
  }

  for(auto i = dialogs_.begin(); i != dialogs_.end(); ++i)
  {
    destroyDialog(i->first);
  }
  dialogs_.clear();
  nextSessionID_ = FIRSTSESSIONID;
}


bool SIPTransactions::identifySession(SIPRequest request,
                                      QHostAddress localAddress,
                                      uint32_t& out_sessionID)
{
  qDebug() << "Starting to process identifying SIP Request session for type:"
           << request.type;

  out_sessionID = 0;

  dialogMutex_.lock();
  for (auto i = dialogs_.begin(); i != dialogs_.end(); ++i)
  {
    if(i->second != nullptr &&
       i->second->state->correctRequestDialog(request.message->dialog,
                                              request.type,
                                              request.message->cSeq))
    {
      qDebug() << "Found dialog matching for incoming request.";
      out_sessionID = i->first;
    }
  }
  dialogMutex_.unlock();

  // find the dialog which corresponds to the callID and tags received in request
  std::shared_ptr<SIPDialogData> foundDialog;

  // we did not find existing dialog for this request
  if(out_sessionID == 0)
  {
    qDebug() << "Could not find the dialog of the request.";

    // TODO: there is a problem if the sequence number did not match
    // and the request type is INVITE
    if(request.type == SIP_INVITE)
    {
      qDebug() << "Someone is trying to start a SIP dialog with us!";

      out_sessionID = createDialogFromINVITE(localAddress, request.message);
      return true;
    }
    return false;
  }
  return true;
}


bool SIPTransactions::identifySession(SIPResponse response, uint32_t& out_sessionID)
{
  qDebug() << "Attempting to identify SIP response session. Response type:"
           << response.type;

  out_sessionID = 0;
  // find the dialog which corresponds to the callID and tags received in response
  for (auto i = dialogs_.begin(); i != dialogs_.end(); ++i)
  {
    if(i->second != nullptr &&
       i->second->state->correctResponseDialog(response.message->dialog,
                                               response.message->cSeq))
    {
      // TODO: we should check that every single detail is as specified in rfc.
      if(i->second->client->waitingResponse(response.message->transactionRequest))
      {
        qDebug() << "Found dialog matching the response";
        out_sessionID = i->first;
        break;
      }
      else
      {
        qDebug() << "PEER_ERROR: Found the dialog, "
                    "but we have not sent a request to their response.";
        return false;
      }
    }
  }

  return out_sessionID != 0;
}


void SIPTransactions::processSIPRequest(SIPRequest request, uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());
  qDebug() << "Starting to process received SIP Request type:" << request.type;

  dialogMutex_.lock();

  // find the dialog which corresponds to the callID and tags received in request
  std::shared_ptr<SIPDialogData> foundDialog;
  foundDialog = dialogs_[sessionID];
  dialogMutex_.unlock();

  // check correct initialization
  Q_ASSERT(foundDialog->state);

  if(!foundDialog->server->processRequest(request, foundDialog->state))
  {
    qDebug() << "Ending session because server Transaction said to.";

    // TODO: SHould we do something?
  }

  qDebug() << "Finished processing request:" << request.type << "Dialog:" << sessionID;
}


void SIPTransactions::processSIPResponse(SIPResponse response, uint32_t sessionID)
{
  Q_ASSERT(sessionID);
  if (sessionID == 0)
  {
    printDebug(DEBUG_PROGRAM_ERROR, this, DC_RECEIVE_SIP_RESPONSE,
               "SessionID was 0 in processSIPResponse. Should be detected earlier.");
    return;
  }

  qDebug() << "Starting to process received SIP Response:" << response.type;
  dialogMutex_.lock();

  // check correct initialization
  Q_ASSERT(dialogs_[sessionID]->state);

  dialogMutex_.unlock();

  // TODO: if our request was INVITE and response is 2xx or 101-199, create dialog
  // TODO: prechecks that the response is ok, then modify program state.

  if (response.type == SIP_OK && response.message->transactionRequest == SIP_INVITE)
  {
    dialogs_[sessionID]->state->setRemoteContactAddress(response.message->contact);
  }

  if(!dialogs_[sessionID]->client->processResponse(response, dialogs_[sessionID]->state))
  {
    // destroy dialog
    destroyDialog(sessionID);
    removeDialog(sessionID);
  }

  if (!response.message->recordRoutes.empty())
  {
    dialogs_[sessionID]->state->setRoute(response.message->recordRoutes);
  }

  qDebug() << "Response processing finished:" << response.type << "Dialog:" << sessionID;
}


void SIPTransactions::sendDialogRequest(uint32_t sessionID, RequestType type)
{
  qDebug() << "---- Iniated sending of a dialog request:" << type << "----";
  Q_ASSERT(sessionID != 0 && dialogs_.find(sessionID) != dialogs_.end());
  // Get all the necessary information from different components.

  SIPRequest request;
  request.type = type;

  // Get message info
  dialogs_[sessionID]->client->getRequestMessageInfo(type, request.message);
  dialogs_[sessionID]->client->startTimer(type);

  // TODO: maybe get the local address from dialogState contact address instead?
  dialogs_[sessionID]->state->getRequestDialogInfo(request);

  Q_ASSERT(request.message != nullptr);
  Q_ASSERT(request.message->dialog != nullptr);


  emit transportRequest(sessionID, request);
  qDebug() << "---- Finished sending of a dialog request ---";
}


void SIPTransactions::sendResponse(uint32_t sessionID, ResponseType type)
{
  qDebug() << "---- Iniated sending of a response:" << type << "----";
  Q_ASSERT(sessionID != 0 && dialogs_.find(sessionID) != dialogs_.end());

  // Get all the necessary information from different components.
  SIPResponse response;
  response.type = type;
  dialogs_[sessionID]->server->getResponseMessage(response.message, type);

  if (type == SIP_OK && response.message->transactionRequest == SIP_INVITE)
  {
    response.message->contact = dialogs_[sessionID]->state->getResponseContactURI();
  }

  emit transportResponse(sessionID, response);
  qDebug() << "---- Finished sending of a response ---";
}


void SIPTransactions::destroyDialog(uint32_t sessionID)
{
  Q_ASSERT(sessionID);
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());
  dialogMutex_.lock();
  std::shared_ptr<SIPDialogData> dialog = dialogs_[sessionID];
  dialogMutex_.unlock();

  if(dialog == nullptr)
  {
    printDebug(DEBUG_PROGRAM_ERROR, this, DC_END_CALL,
               "Bad sessionID for destruction.");
    return;
  }
  printDebug(DEBUG_NORMAL, this, DC_END_CALL, "Destroying SIP dialog",
                {"SessionID"}, {QString::number(sessionID)});

  dialog->state.reset();
  dialog->server.reset();
  dialog->client.reset();
}

void SIPTransactions::removeDialog(uint32_t sessionID)
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
