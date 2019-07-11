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
  registrations_(),
  directContactAddresses_(),
  nonDialogClient_(),
  sdp_(),

  transactionUser_(nullptr)
{}

void SIPTransactions::init(SIPTransactionUser *callControl)
{
  qDebug() << "Iniating SIP Transactions";

  transactionUser_ = callControl;
  nonDialogClient_ = std::unique_ptr<SIPNonDialogClient>(new SIPNonDialogClient(callControl));

  QSettings settings("kvazzup.ini", QSettings::IniFormat);
  QString username = !settings.value("local/Username").isNull()
      ? settings.value("local/Username").toString() : "anonymous";

  sdp_.setLocalInfo(username);
}


void SIPTransactions::uninit()
{
  //TODO: delete all dialogs
  for(auto i = dialogs_.begin(); i != dialogs_.end(); ++i)
  {
    destroyDialog(i.key());
  }

  dialogs_.clear();
  nextSessionID_ = FIRSTSESSIONID;
}


void SIPTransactions::registerTask()
{
  qDebug() << "Registering us to a new SIP server with REGISTER task";
}

void SIPTransactions::bindToServer(QString serverAddress, QHostAddress localAddress, uint32_t sessionID)
{
  qDebug() << "Binding to SIP server at:" << serverAddress;

  SIPRegistrationData data = {std::shared_ptr<SIPNonDialogClient> (new SIPNonDialogClient(transactionUser_)),
                             std::shared_ptr<SIPDialogState> (new SIPDialogState()), sessionID, localAddress};

  registrations_[serverAddress] = data;

  SIP_URI serverUri = {"","",serverAddress, SIP};
  registrations_[serverAddress].state->createServerDialog(serverUri);
  registrations_[serverAddress].client->init();


  registrations_[serverAddress].client->set_remoteURI(serverUri);

  sendNonDialogRequest(serverUri, SIP_REGISTER);
}


void SIPTransactions::startDirectCall(Contact &address, QHostAddress localAddress,
                                      uint32_t sessionID)
{
  // There should not exist a dialog for this user
  if(!sdp_.canStartSession())
  {
    qDebug() << "Not enough ports to start a call";
    return;
  }

  startPeerToPeerCall(sessionID, localAddress, address);
}

void SIPTransactions::startProxyCall(Contact& address, QHostAddress localAddress,
                                     uint32_t sessionID)
{
  Q_UNUSED(address);
  Q_UNUSED(localAddress);
  Q_UNUSED(sessionID);
  printDebug(DEBUG_ERROR, this, DC_START_CALL,
             "Proxy calling has not been yet implemented");
}

void SIPTransactions::startPeerToPeerCall(uint32_t sessionID,
                                          QHostAddress localAddress, Contact &remote)
{
  qDebug() << "SIP," << metaObject()->className() << ": Intializing a new dialog with INVITE";

  std::shared_ptr<SIPDialogData> dialogData;
  createBaseDialog(sessionID, localAddress, dialogData);
  dialogData->proxyConnection_ = false;
  dialogData->state->createNewDialog(SIP_URI{remote.username, remote.username, remote.remoteAddress, SIP});

  // this start call will commence once the connection has been established
  if(!dialogData->client->startCall(remote.realName))
  {
    printDebug(DEBUG_WARNING, this, DC_START_CALL,
               "Could not start a call according to session.");
  }
}

uint32_t SIPTransactions::createDialogFromINVITE(QHostAddress localAddress,
                                                 std::shared_ptr<SIPMessageInfo> &invite)
{
  Q_ASSERT(invite);

  uint32_t sessionID = reserveSessionID();

  std::shared_ptr<SIPDialogData> dialog;
  createBaseDialog(sessionID, localAddress, dialog);

  dialog->state->createDialogFromINVITE(invite);
  dialog->state->setPeerToPeerHostname(localAddress.toString(), false);
  return sessionID;
}

void SIPTransactions::createBaseDialog(uint32_t sessionID,
                                       QHostAddress& localAddress,
                                       std::shared_ptr<SIPDialogData>& dialog)
{
  dialog = std::shared_ptr<SIPDialogData> (new SIPDialogData);
  dialogMutex_.lock();
  dialog->localAddress = localAddress;

  dialog->state = std::shared_ptr<SIPDialogState> (new SIPDialogState());

  dialog->client = std::shared_ptr<SIPDialogClient> (new SIPDialogClient(transactionUser_));
  dialog->client->init();
  dialog->client->setSessionID(sessionID);

  dialog->server = std::shared_ptr<SIPServerTransaction> (new SIPServerTransaction);
  dialog->server->init(transactionUser_, sessionID);

  dialog->proxyConnection_ = false;

  dialogs_[sessionID] = dialog;

  dialogMutex_.unlock();

  QObject::connect(dialog->client.get(), &SIPDialogClient::sendDialogRequest,
                   this, &SIPTransactions::sendDialogRequest);

  QObject::connect(dialog->server.get(), &SIPServerTransaction::sendResponse,
                   this, &SIPTransactions::sendResponse);
}

void SIPTransactions::acceptCall(uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());

  qDebug() << "Accept, SIPTransaction" << "accepting call:" << sessionID;

  dialogMutex_.lock();
  std::shared_ptr<SIPDialogData> dialog = dialogs_[sessionID];
  dialogMutex_.unlock();

  // start candiate nomination. This function won't block, negotiation happens in the background
  // remoteFinalSDP() makes sure that a connection was in fact nominated
  sdp_.startICECandidateNegotiation(sessionID);

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
  for(auto dialog : dialogs_)
  {
    if(dialog != nullptr)
    {
      dialog->client->endCall();
    }
  }

  for(auto i = dialogs_.begin(); i != dialogs_.end(); ++i)
  {
    destroyDialog(i.key());
  }
  dialogs_.clear();
  nextSessionID_ = FIRSTSESSIONID;
}


void SIPTransactions::getSDPs(uint32_t sessionID,
                              std::shared_ptr<SDPMessageInfo>& localSDP,
                              std::shared_ptr<SDPMessageInfo>& remoteSDP) const
{
  Q_ASSERT(sessionID);
  localSDP = sdp_.getLocalSDP(sessionID);
  remoteSDP = sdp_.getRemoteSDP(sessionID);
}


bool SIPTransactions::identifySession(SIPRequest request, QHostAddress localAddress,
                                      uint32_t& out_sessionID)
{
  qDebug() << "Starting to process received SIP Request:" << request.type;

  out_sessionID = 0;

  dialogMutex_.lock();
  for (auto i = dialogs_.begin(); i != dialogs_.end(); ++i)
  {
    if(i.value() != nullptr &&
       i.value()->state->correctRequestDialog(request.message->dialog,
                                              request.type, request.message->cSeq))
    {
      qDebug() << "Found dialog matching for incoming request.";
      out_sessionID = i.key();
    }
  }
  dialogMutex_.unlock();

  // find the dialog which corresponds to the callID and tags received in request
  std::shared_ptr<SIPDialogData> foundDialog;

  // we did not find existing dialog for this request
  if(out_sessionID == 0)
  {
    qDebug() << "Could not find the dialog of the request.";

    // TODO: there is a problem if the sequence number did not match and the request type is INVITE
    if(request.type == SIP_INVITE)
    {
      qDebug() << "Someone is trying to start a SIP dialog with us!";

      if(!sdp_.canStartSession())
      {
        qDebug() << "Not enough free media ports to accept new dialog";
        return false;
      }

      out_sessionID = createDialogFromINVITE(localAddress, request.message);
      return true;
    }
    else
    {
      //printDebug(DEBUG_PEER_ERROR, metaObject()->className(),
      //           DC_RECEIVE_SIP_REQUEST,"Couldn't find the correct dialog!");
      return false;
    }
  }
  return false;
}


void SIPTransactions::processSIPRequest(SIPRequest request, QHostAddress localAddress,
                                        QVariant &content, uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());
  qDebug() << "Starting to process received SIP Request:" << request.type;

  dialogMutex_.lock();

  // find the dialog which corresponds to the callID and tags received in request
  std::shared_ptr<SIPDialogData> foundDialog;
  foundDialog = dialogs_[sessionID];
  dialogMutex_.unlock();

  // check correct initialization
  Q_ASSERT(foundDialog->state);

  // TODO: prechecks that the message is ok, then modify program state.
  if(request.type == SIP_INVITE || request.type == SIP_ACK)
  {
    if(request.message->content.type == APPLICATION_SDP)
    {
      if(request.type == SIP_INVITE)
      {
        if(!processSDP(sessionID, content, localAddress))
        {
          qDebug() << "Failed to find suitable SDP.";
          foundDialog->server->setCurrentRequest(request); // TODO: crashes
          sendResponse(sessionID, SIP_DECLINE, request.type);
          return;
        }
      }
      else //ACK
      {
        if(!content.isValid())
        {
          printDebug(DEBUG_PROGRAM_ERROR, this, DC_RECEIVE_SIP_REQUEST,
                           "The SDP content is not valid at processing. Should be detected earlier.");
          return;
        }

        SDPMessageInfo retrieved = content.value<SDPMessageInfo>();

        // remoteFinalSDP blocks until the ICE has finished its job
        //
        // After it returns, we add the nominated media connections to local and remote SDPs
        if(!sdp_.verifyResponseSDP(retrieved, sessionID))
        {
          qDebug() << "PEER_ERROR:" << "Their final sdp is not suitable. They should have followed our SDP!!!";
          return;
        }
        else
        {
          sdp_.setICEPorts(sessionID);
        }
      }
    }
    else
    {
      qDebug() << "PEER ERROR: No SDP in" << request.type;
      // TODO: set the request details to serverTransaction
      sendResponse(sessionID, SIP_DECLINE, request.type);
      return;
    }
  }

  if(!foundDialog->server->processRequest(request))
  {
    qDebug() << "Ending session because server said to";
    sdp_.endSession(sessionID);
  }

  qDebug() << "Finished processing request:" << request.type << "Dialog:" << sessionID;
}


void SIPTransactions::processSIPResponse(SIPResponse response,
                                         QHostAddress localAddress,
                                         QVariant &content)
{
  // TODO: sessionID is now tranportID
  // TODO: separate nondialog and dialog requests!
  qDebug() << "Starting to process received SIP Response:" << response.type;
  dialogMutex_.lock();

  // find the dialog which corresponds to the callID and tags received in response
  uint32_t foundSessionID = 0;

  for (auto i = dialogs_.begin(); i != dialogs_.end(); ++i)
  {
    if(i.value() != nullptr &&
       i.value()->state->correctResponseDialog(response.message->dialog,
                                               response.message->cSeq))
    {
      // TODO: we should check that every single detail is as specified in rfc.
      if(i.value()->client->waitingResponse(response.message->transactionRequest))
      {
        qDebug() << "Found dialog matching the response";
        foundSessionID = i.key();
        break;
      }
      else
      {
        qDebug() << "PEER_ERROR: Found the dialog, but we have not sent a request to their response.";
      }
    }
  }

  if(foundSessionID == 0)
  {
    qDebug() << "PEER_ERROR: Could not find the suggested dialog in response!";
    qDebug() << "CallID:" << response.message->dialog->callID;
    dialogMutex_.unlock();
    return;
  }

  // check correct initialization
  Q_ASSERT(dialogs_[foundSessionID]->state);

  dialogMutex_.unlock();

  // TODO: if our request was INVITE and response is 2xx or 101-199, create dialog
  // TODO: prechecks that the response is ok, then modify program state.

  if(response.message->transactionRequest == SIP_INVITE && response.type == SIP_OK)
  {
    if(response.message->content.type == APPLICATION_SDP)
    {
      if(!processSDP(foundSessionID, content, localAddress))
      {
        qDebug() << "Failed to find suitable SDP in INVITE response.";
        return;
      }
    }
  }

  if(!dialogs_[foundSessionID]->client->processResponse(response))
  {
    // destroy dialog
    destroyDialog(foundSessionID);
    removeDialog(foundSessionID);
  }
  qDebug() << "Response processing finished:" << response.type << "Dialog:" << foundSessionID;
}

bool SIPTransactions::processSDP(uint32_t sessionID, QVariant& content, QHostAddress localAddress)
{
  if(!content.isValid() || dialogs_.find(sessionID) == dialogs_.end())
  {
    printDebug(DEBUG_PROGRAM_ERROR, this, DC_SIP_CONTENT,
                     "The SDP content is not valid at processing. Should be detected earlier.");
    return false;
  }

  SDPMessageInfo retrieved = content.value<SDPMessageInfo>();

  if(!sdp_.generateResponseSDP(retrieved, localAddress, sessionID))
  {
    qDebug() << "Remote SDP not suitable or we have no ports to assign";
    destroyDialog(sessionID);
    removeDialog(sessionID);
    return false;
  }
  return true;
}

void SIPTransactions::sendDialogRequest(uint32_t sessionID, RequestType type)
{
  qDebug() << "---- Iniated sending of a dialog request:" << type << "----";
  Q_ASSERT(sessionID != 0 && dialogs_.find(sessionID) != dialogs_.end());
  // Get all the necessary information from different components.

  SIPRequest request;
  request.type = type;

  // if this is the session creation INVITE. Proxy sessions should be created earlier.
  if(request.type == SIP_INVITE && !dialogs_[sessionID]->proxyConnection_)
  {
    dialogs_[sessionID]->state->setPeerToPeerHostname(dialogs_[sessionID]->localAddress.toString());
  }

  // Get message info
  dialogs_[sessionID]->client->getRequestMessageInfo(type, request.message);
  dialogs_[sessionID]->client->startTimer(type);

  dialogs_[sessionID]->state->getRequestDialogInfo(request,
                                                   dialogs_[sessionID]->localAddress.toString());

  Q_ASSERT(request.message != nullptr);
  Q_ASSERT(request.message->dialog != nullptr);

  QVariant content;
  request.message->content.length = 0;
  if(type == SIP_INVITE)
  {
    qDebug() << "Adding SDP content to request:" << type;
    request.message->content.type = APPLICATION_SDP;
    SDPMessageInfo sdp;

    if(type == SIP_INVITE)
    {
      if(sdp_.generateInitialSDP(dialogs_[sessionID]->localAddress, sessionID))
      {
        sdp = *sdp_.getLocalSDP(sessionID);
      }
      else
      {
        qDebug() << "Failed to generate SDP Suggestion while sending: " << type <<
                    "Possibly because we ran out of ports to assign";

        return;
      }
    }
    content.setValue(sdp);
  }

  emit transportRequest(sessionID, request, content);
  qDebug() << "---- Finished sending of a dialog request ---";
}

void SIPTransactions::sendNonDialogRequest(SIP_URI& uri, RequestType type)
{
  qDebug() << "Start sending of a non-dialog request. Type:" << type;
  SIPRequest request;
  request.type = type;

  if (type == SIP_REGISTER)
  {
    if (registrations_.find(uri.host) == registrations_.end())
    {
      printDebug(DEBUG_PROGRAM_ERROR, this, DC_SEND_SIP_REQUEST,
                 "Registration information should have been created "
                 "already before sending REGISTER message!");

      return;
    }

    registrations_[uri.host].client->getRequestMessageInfo(request.type, request.message);
    registrations_[uri.host].state->getRequestDialogInfo(request, registrations_[uri.host].localAddress.toString());

    QVariant content; // we dont have content in REGISTER
    emit transportRequest(registrations_[uri.host].sessionID, request, content);
  }
  else if (type == SIP_OPTIONS) {
    printDebug(DEBUG_PROGRAM_ERROR, this, DC_SEND_SIP_REQUEST,
                     "Trying to send unimplemented non-dialog request OPTIONS!");
  }
  else {
    printDebug(DEBUG_PROGRAM_ERROR, this, DC_SEND_SIP_REQUEST,
                     "Trying to send a non-dialog request of type which is a dialog request!");
  }
}

void SIPTransactions::sendResponse(uint32_t sessionID, ResponseType type, RequestType originalRequest)
{
  qDebug() << "---- Iniated sending of a response:" << type << "----";
  Q_ASSERT(sessionID != 0 && dialogs_.find(sessionID) != dialogs_.end());

  // Get all the necessary information from different components.
  SIPResponse response;
  response.type = type;
  dialogs_[sessionID]->server->getResponseMessage(response.message, type);
  response.message->transactionRequest = originalRequest;

  QVariant content;
  if(response.message->transactionRequest == SIP_INVITE && type == SIP_OK) // TODO: SDP in progress...
  {
    response.message->content.type = APPLICATION_SDP;
    std::shared_ptr<SDPMessageInfo> sdp = sdp_.getLocalSDP(sessionID);
    content.setValue(*sdp);
  }

  emit transportResponse(sessionID, response, content);
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
  qDebug() << "Destroying dialog:";

  sdp_.endSession(sessionID);
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
