#include "sip/siptransactions.h"

#include "sip/sipdialogstate.h"
#include "sip/siptransport.h"

#include "sip/sipdialogclient.h"
#include "sip/sipservertransaction.h"


SIPTransactions::SIPTransactions():
  pendingConnectionMutex_(),
  pendingDialogRequests_(),
  pendingNonDialogRequests_(),
  dialogs_(),
  registrations_(),
  transports_(),
  directContactAddresses_(),
  nonDialogClient_(),
  sdp_(),
  tcpServer_(),
  sipPort_(5060), // default for SIP, use 5061 for tls encrypted
  transactionUser_(nullptr)
{}

void SIPTransactions::init(SIPTransactionUser *callControl)
{
  qDebug() << "Iniating SIP Manager";

  transactionUser_ = callControl;
  nonDialogClient_ = std::unique_ptr<SIPNonDialogClient>(new SIPNonDialogClient(callControl));

  QObject::connect(&tcpServer_, &ConnectionServer::newConnection,
                   this, &SIPTransactions::receiveTCPConnection);

  // listen to everything
  tcpServer_.listen(QHostAddress("0.0.0.0"), sipPort_);
  QSettings settings("kvazzup.ini", QSettings::IniFormat);
  QString username = !settings.value("local/Username").isNull()
      ? settings.value("local/Username").toString() : "anonymous";

  sdp_.setLocalInfo(username);
}


void SIPTransactions::uninit()
{
  //TODO: delete all dialogs
  for(uint32_t sessionID = 1; sessionID - 1 < dialogs_.size(); ++sessionID)
  {
    destroyDialog(sessionID);
  }

  dialogs_.clear();

  for(std::shared_ptr<SIPTransport> transport : transports_)
  {
    if(transport != nullptr)
    {
      transport->cleanup();
      transport.reset();
    }
  }
}

bool SIPTransactions::isConnected(QString remoteAddress, quint32& transportID)
{
  for(int j = 0; j < transports_.size(); ++j)
  {
    if(transports_.at(j) != nullptr &&
       transports_.at(j)->getRemoteAddress().toString() == remoteAddress)
    {
      transportID = j + 1;
      return true;
    }
  }
  return false;
}

void SIPTransactions::registerTask()
{
  qDebug() << "Registering us to a new SIP server with REGISTER task";
}

void SIPTransactions::bindToServer()
{
  // get server address from settings and bind to server.
  QSettings settings("kvazzup.ini", QSettings::IniFormat);

  QString serverAddress = settings.value("sip/ServerAddress").toString();

  if (serverAddress != "")
  {
    qDebug() << "Binding to SIP server at:" << serverAddress;

    SIPRegistrationData data = {std::shared_ptr<SIPNonDialogClient> (new SIPNonDialogClient(transactionUser_)),
                               std::shared_ptr<SIPDialogState> (new SIPDialogState()), 0};
    registrations_[serverAddress] = data;

    std::shared_ptr<SIPTransport> transport = createSIPTransport();
    transport->createConnection(TCP, serverAddress);

    registrations_[serverAddress].transportID = transports_.size();

    SIP_URI serverUri = {"","",serverAddress};
    registrations_[serverAddress].state->createServerDialog(serverUri);
    registrations_[serverAddress].client->init();


    registrations_[serverAddress].client->set_remoteURI(serverUri);

    sendNonDialogRequest(serverUri, SIP_REGISTER);
  }
  else {
    qDebug() << "SIP server address was empty in settings";
  }
}

void SIPTransactions::getSDPs(uint32_t sessionID,
                              std::shared_ptr<SDPMessageInfo>& localSDP,
                              std::shared_ptr<SDPMessageInfo>& remoteSDP)
{
  Q_ASSERT(sessionID != 0);

  if(dialogs_.at(sessionID - 1)->localSdp_ == nullptr ||
     dialogs_.at(sessionID - 1)->remoteSdp_ == nullptr)
  {
    qDebug() << "getSDP: Both SDP:s are not present for some reason."
             << "Maybe the call has ended before starting?";
  }

  localSDP = dialogs_.at(sessionID - 1)->localSdp_;
  remoteSDP = dialogs_.at(sessionID - 1)->remoteSdp_;
}


void SIPTransactions::startCall(Contact &address)
{
  // There should not exist a dialog for this user

  if(!sdp_.canStartSession())
  {
    qDebug() << "Not enough ports to start a call";
    return;
  }

  // is the address at one of the servers we are connected to
  bool isServer = registrations_.find(address.remoteAddress) != registrations_.end();

  if(isServer)
  {
    qDebug() << "ERROR: Proxy calling has not been yet implemented";
  }
  else
  {
    // If we are not connected to the server, try to get the ip address and form a direct connection.

    // check if we are already connected to their adddress
    quint32 transportID = 0;
    if(isConnected(address.remoteAddress, transportID)) // sets the transportid
    {
      qDebug() << "Found existing connection to this address.";
      startPeerToPeerCall(transportID, address);
    }
    else
    {
      std::shared_ptr<SIPTransport> transport = createSIPTransport();
      transport->createConnection(TCP, address.remoteAddress);
      startPeerToPeerCall(transports_.size(), address);

      //dialogData->transportID = transports_.size();
    }
  }
}

void SIPTransactions::startPeerToPeerCall(quint32 transportID, Contact &remote)
{
  qDebug() << "SIP," << metaObject()->className() << ": Intializing a new dialog with INVITE";

  std::shared_ptr<SIPDialogData> dialogData;
  createBaseDialog(transportID, dialogData);
  dialogData->proxyConnection_ = false;
  dialogData->state->createNewDialog(SIP_URI{remote.username, remote.username, remote.remoteAddress});

  // this start call will commence once the connection has been established
  if(!dialogData->client->startCall(remote.realName))
  {
    qDebug() << "WARNING: Could not start a call according to session.";
  }
}

void SIPTransactions::createDialogFromINVITE(quint32 transportID, std::shared_ptr<SIPMessageInfo> &invite,
                                             std::shared_ptr<SIPDialogData>& dialog)
{
  Q_ASSERT(invite);

  createBaseDialog(transportID, dialog);

  dialog->state->createDialogFromINVITE(invite);
  dialog->state->setPeerToPeerHostname(transports_.at(transportID - 1)->getLocalAddress().toString(), false);
}

void SIPTransactions::createBaseDialog( quint32 transportID, std::shared_ptr<SIPDialogData>& dialog)
{
  Q_ASSERT(transportID <= transports_.size());

  dialog = std::shared_ptr<SIPDialogData> (new SIPDialogData);
  connectionMutex_.lock();
  dialog->transportID = transportID;

  dialog->state = std::shared_ptr<SIPDialogState> (new SIPDialogState());

  dialog->client = std::shared_ptr<SIPDialogClient> (new SIPDialogClient(transactionUser_));
  dialog->client->init();
  dialog->client->setSessionID(dialogs_.size() + 1);

  dialog->server = std::shared_ptr<SIPServerTransaction> (new SIPServerTransaction);
  dialog->server->init(transactionUser_, dialogs_.size() + 1);

  dialog->localSdp_ = nullptr;
  dialog->remoteSdp_ = nullptr;
  QObject::connect(dialog->client.get(), &SIPDialogClient::sendDialogRequest,
                   this, &SIPTransactions::sendDialogRequest);

  QObject::connect(dialog->server.get(), &SIPServerTransaction::sendResponse,
                   this, &SIPTransactions::sendResponse);

  dialogs_.push_back(dialog);
  connectionMutex_.unlock();
}

void SIPTransactions::acceptCall(uint32_t sessionID)
{
  connectionMutex_.lock();
  std::shared_ptr<SIPDialogData> dialog = dialogs_.at(sessionID - 1);
  connectionMutex_.unlock();
  dialog->server->acceptCall();
}

void SIPTransactions::rejectCall(uint32_t sessionID)
{
  connectionMutex_.lock();
  std::shared_ptr<SIPDialogData> dialog = dialogs_.at(sessionID - 1);
  connectionMutex_.unlock();
  dialog->server->rejectCall();
  destroyDialog(sessionID);
}

void SIPTransactions::endCall(uint32_t sessionID)
{
  std::shared_ptr<SIPDialogData> dialog = dialogs_.at(sessionID - 1);
  dialog->client->endCall();
  destroyDialog(sessionID);
}

void SIPTransactions::cancelCall(uint32_t sessionID)
{
  std::shared_ptr<SIPDialogData> dialog = dialogs_.at(sessionID - 1);
  dialog->client->cancelCall();
  destroyDialog(sessionID);
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

  for(int i = 0; i < dialogs_.size(); ++i)
  {
    if(dialogs_.at(i) != nullptr)
    {
      destroyDialog(i + 1);
    }
  }
}

std::shared_ptr<SIPTransport> SIPTransactions::createSIPTransport()
{
  std::shared_ptr<SIPTransport> connection =
      std::shared_ptr<SIPTransport>(new SIPTransport(transports_.size() + 1));

  QObject::connect(connection.get(), &SIPTransport::incomingSIPRequest,
                   this, &SIPTransactions::processSIPRequest);
  QObject::connect(connection.get(), &SIPTransport::incomingSIPResponse,
                   this, &SIPTransactions::processSIPResponse);

  QObject::connect(connection.get(), &SIPTransport::sipTransportEstablished,
                   this, &SIPTransactions::connectionEstablished);
  transports_.push_back(connection);
  return connection;
}

void SIPTransactions::receiveTCPConnection(TCPConnection *con)
{
  qDebug() << "Received a TCP connection. Initializing dialog";
  Q_ASSERT(con);

  std::shared_ptr<SIPTransport> transport = createSIPTransport();
  transport->incomingTCPConnection(std::shared_ptr<TCPConnection> (con));

  qDebug() << "Dialog with ID:" << dialogs_.size() << "created for received connection.";
}

void SIPTransactions::connectionEstablished(quint32 transportID)
{
  qDebug() << "A SIP connection has been established as we wanted. TransportID:" << transportID;
  pendingConnectionMutex_.lock();

  bool pendingNonDialog = pendingNonDialogRequests_.find(transportID) != pendingNonDialogRequests_.end();
  bool pendingDialog = pendingDialogRequests_.find(transportID) != pendingDialogRequests_.end();

  pendingConnectionMutex_.unlock();

  if(pendingNonDialog)
  {
    qDebug() << "We have a pending non-dialog request. Sending it!";
    NonDialogRequest request = pendingNonDialogRequests_[transportID];
    sendNonDialogRequest(request.request_uri, request.type);
    pendingNonDialogRequests_.erase(transportID);
  }

  if(pendingDialog)
  {
    qDebug() << "We have a pending dialog request. Sending it!";
    DialogRequest request = pendingDialogRequests_[transportID];
    sendDialogRequest(request.sessionID, request.type);
    pendingDialogRequests_.erase(transportID);
  }
}

void SIPTransactions::processSIPRequest(SIPRequest request,
                       quint32 transportID, QVariant &content)
{
  qDebug() << "Starting to process received SIP Request:" << request.type;

  // TODO: sessionID is now tranportID
  // TODO: separate non-dialog and dialog requests!
  connectionMutex_.lock();

  uint32_t foundSessionID = 0;
  for(unsigned int sessionID = 1; sessionID - 1 < dialogs_.size(); ++sessionID)
  {
    if(dialogs_.at(sessionID - 1) != nullptr &&
       dialogs_.at(sessionID - 1)->state->correctRequestDialog(request.message->dialog,
                                                                request.type,
                                                                request.message->cSeq))
    {
      qDebug() << "Found dialog matching for incoming request.";
      foundSessionID = sessionID;
    }
  }

  // find the dialog which corresponds to the callID and tags received in request
  std::shared_ptr<SIPDialogData> foundDialog;
  connectionMutex_.unlock();

  if(foundSessionID == 0)
  {
    qDebug() << "Could not find the dialog of the request.";

    // TODO: there is a problem if the sequence number did not match and the request type is INVITE
    if(request.type == SIP_INVITE)
    {
      qDebug() << "Someone is trying to start a sip dialog with us!";

      if(!sdp_.canStartSession())
      {
        qDebug() << "Not enough free media ports to accept new dialog";
        return;
      }

      createDialogFromINVITE(transportID, request.message, foundDialog);
      foundSessionID = dialogs_.size();

      // Proxy TODO: somehow distinguish if this is a proxy connection
      foundDialog->proxyConnection_ = false;
    }
    else
    {
      qDebug() << "PEER_ERROR: Couldn't find the correct dialog!";
      // TODO: send correct response.
      return;
    }
  }
  else
  {
    Q_ASSERT(foundSessionID <= dialogs_.size());
    foundDialog = dialogs_.at(foundSessionID - 1);
  }

  // check correct initialization
  Q_ASSERT(foundDialog->state);

  // TODO: prechecks that the message is ok, then modify program state.
  if(request.type == SIP_INVITE || request.type == SIP_ACK)
  {
    if(request.message->content.type == APPLICATION_SDP)
    {
      if(request.type == SIP_INVITE)
      {
        if(!processSDP(foundSessionID, content, transports_.at(transportID - 1)->getLocalAddress()))
        {
          qDebug() << "Failed to find suitable SDP.";
          foundDialog->server->setCurrentRequest(request); // TODO: crashes
          sendResponse(foundSessionID, SIP_DECLINE, request.type);
          return;
        }
      }
      else //ACK
      {
        if(!content.isValid())
        {
          qWarning() << "ERROR: The SDP content is not valid at processing. Should be detected earlier.";
          return;
        }

        SDPMessageInfo retrieved = content.value<SDPMessageInfo>();

        if(!sdp_.remoteFinalSDP(retrieved))
        {
          qDebug() << "PEER_ERROR:" << "Their final sdp is not suitable. They should have followed our SDP!!!";
          return;
        }
      }
    }
    else
    {
      qDebug() << "PEER ERROR: No SDP in" << request.type;
      // TODO: set the request details to serverTransaction
      sendResponse(transportID, SIP_DECLINE, request.type);
      return;
    }
  }

  if(!foundDialog->server->processRequest(request))
  {
    qDebug() << "Ending session because server said to";
    sdp_.endSession(foundDialog->localSdp_);
    foundDialog->localSdp_ = nullptr;
  }

  qDebug() << "Finished processing request:" << request.type << "Dialog:" << foundSessionID;
}

void SIPTransactions::processSIPResponse(SIPResponse response,
                                         quint32 transportID, QVariant &content)
{
  // TODO: sessionID is now tranportID
  // TODO: separate nondialog and dialog requests!
  qDebug() << "Starting to process received SIP Response:" << response.type;
  connectionMutex_.lock();

  // find the dialog which corresponds to the callID and tags received in response
  uint32_t foundSessionID = 0;

  for(unsigned int sessionID = 1; sessionID - 1 < dialogs_.size(); ++sessionID)
  {
    if(dialogs_.at(sessionID - 1) != nullptr &&
       dialogs_.at(sessionID - 1)->state->correctResponseDialog(response.message->dialog,
                                             response.message->cSeq))
    {
      // TODO: we should check that every single detail is as specified in rfc.
      if(dialogs_.at(sessionID - 1)->client->waitingResponse(response.message->transactionRequest))
      {
        qDebug() << "Found dialog matching the response";
        foundSessionID = sessionID;
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
    qDebug() << "TransportID:" << transportID << "CallID:" << response.message->dialog->callID;
    return;
  }

  // check correct initialization
  Q_ASSERT(dialogs_.at(foundSessionID - 1)->state);

  connectionMutex_.unlock();

  // TODO: if our request was INVITE and response is 2xx or 101-199, create dialog
  // TODO: prechecks that the response is ok, then modify program state.

  if(response.message->transactionRequest == SIP_INVITE && response.type == SIP_OK)
  {
    if(response.message->content.type == APPLICATION_SDP)
    {
      if(!processSDP(foundSessionID, content, transports_.at(transportID - 1)->getLocalAddress()))
      {
        qDebug() << "Failed to find suitable SDP in INVITE response.";
        return;
      }
    }
  }

  if(!dialogs_.at(foundSessionID - 1)->client->processResponse(response))
  {
    // destroy dialog
    destroyDialog(foundSessionID);
  }
  qDebug() << "Response processing finished:" << response.type << "Dialog:" << foundSessionID;
}

bool SIPTransactions::processSDP(uint32_t sessionID, QVariant& content, QHostAddress localAddress)
{
  if(!content.isValid())
  {
    qWarning() << "ERROR: The SDP content is not valid at processing. Should be detected earlier.";
    return false;
  }

  SDPMessageInfo retrieved = content.value<SDPMessageInfo>();

  dialogs_.at(sessionID - 1)->localSdp_
      = sdp_.localFinalSDP(retrieved, localAddress, dialogs_.at(sessionID - 1)->localSdp_);

  if(dialogs_.at(sessionID - 1)->localSdp_ == nullptr)
  {
    qDebug() << "Remote SDP not suitable or we have no ports to assign";
    destroyDialog(sessionID);
    return false;
  }
  dialogs_.at(sessionID - 1)->remoteSdp_ = std::shared_ptr<SDPMessageInfo> (new SDPMessageInfo);
  *dialogs_.at(sessionID - 1)->remoteSdp_ = retrieved;
  return true;
}

void SIPTransactions::sendDialogRequest(uint32_t sessionID, RequestType type)
{
  qDebug() << "---- Iniated sending of a dialog request:" << type << "----";
  Q_ASSERT(sessionID != 0 && sessionID <= dialogs_.size());
  Q_ASSERT(dialogs_.at(sessionID - 1)->transportID - 1 != 0);
  // Get all the necessary information from different components.

  std::shared_ptr<SIPTransport> transport
      = transports_.at(dialogs_.at(sessionID - 1)->transportID - 1);

  pendingConnectionMutex_.lock();
  if(!transport->isConnected())
  {
    qDebug() << "SIP," << metaObject()->className() << "The connection has not yet been established. Delaying sending of request.";

    pendingDialogRequests_[dialogs_.at(sessionID - 1)->transportID] = (DialogRequest{sessionID, type});
    pendingConnectionMutex_.unlock();
    return;
  }
  pendingConnectionMutex_.unlock();

  SIPRequest request;
  request.type = type;

  // if this is the session creation INVITE. Proxy sessions should be created earlier.
  if(request.type == SIP_INVITE && !dialogs_.at(sessionID - 1)->proxyConnection_)
  {
    dialogs_.at(sessionID - 1)->state->setPeerToPeerHostname(transport->getLocalAddress().toString());
  }

  // Get message info
  dialogs_.at(sessionID - 1)->client->getRequestMessageInfo(type, request.message);
  dialogs_.at(sessionID - 1)->client->startTimer(type);

  dialogs_.at(sessionID - 1)->state->getRequestDialogInfo(request,
                                                          transport->getLocalAddress().toString());

  Q_ASSERT(request.message != nullptr);
  Q_ASSERT(request.message->dialog != nullptr);

  QVariant content;
  request.message->content.length = 0;
  if(type == SIP_INVITE || type == SIP_ACK)
  {
    qDebug() << "Adding SDP content to request:" << type;
    request.message->content.type = APPLICATION_SDP;
    SDPMessageInfo sdp;
    if(type == SIP_INVITE)
    {
      dialogs_.at(sessionID - 1)->localSdp_
          = sdp_.localSDPSuggestion(transport->getLocalAddress());

      if(dialogs_.at(sessionID - 1)->localSdp_ != nullptr)
      {
        sdp = *dialogs_.at(sessionID - 1)->localSdp_.get();
      }
      else
      {
        qDebug() << "Failed to generate SDP Suggestion while sending: " << type <<
                    "Possibly because we ran out of ports to assign";

        return;
      }
    }
    else
    {
      if(dialogs_.at(sessionID - 1)->localSdp_ == nullptr)
      {
        qDebug() << "ERROR: Missing local final SDP when its supposed to be sent.";
        // TODO: send client error.
        return;
      }

      sdp = *dialogs_.at(sessionID - 1)->localSdp_.get();
    }
    content.setValue(sdp);
  }

  transport->sendRequest(request, content);
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
      qDebug() << "ERROR: Registration information should have been created "
                  "already before sending REGISTER message!";
      return;
    }

    std::shared_ptr<SIPTransport> transport
        = transports_.at(registrations_[uri.host].transportID - 1);

    pendingConnectionMutex_.lock();
    if(!transport->isConnected())
    {
      qDebug() << "The connection has not yet been established. Delaying sending of request.";

      pendingNonDialogRequests_[registrations_[uri.host].transportID] = NonDialogRequest{uri, type};
      pendingConnectionMutex_.unlock();
      return;
    }
    pendingConnectionMutex_.unlock();

    registrations_[uri.host].client->getRequestMessageInfo(request.type, request.message);
    registrations_[uri.host].state->getRequestDialogInfo(request, transport->getLocalAddress().toString());

    QVariant content; // we dont have content in REGISTER
    transport->sendRequest(request, content);
  }
  else if (type == SIP_OPTIONS) {
    qWarning() << "ERROR: Trying to send unimplemented non-dialog request OPTIONS!";
  }
  else {
    qWarning() << "ERROR: Trying to send a non-dialog request of type which is a dialog request!";
  }
}

void SIPTransactions::sendResponse(uint32_t sessionID, ResponseType type, RequestType originalRequest)
{
  qDebug() << "---- Iniated sending of a response:" << type << "----";
  Q_ASSERT(sessionID != 0 && sessionID <= dialogs_.size());

  // Get all the necessary information from different components.
  SIPResponse response;
  response.type = type;
  dialogs_.at(sessionID - 1)->server->getResponseMessage(response.message, type);
  response.message->transactionRequest = originalRequest;

  QVariant content;
  if(response.message->transactionRequest == SIP_INVITE && type == SIP_OK) // TODO: SDP in progress...
  {
    response.message->content.type = APPLICATION_SDP;
    SDPMessageInfo sdp = *dialogs_.at(sessionID - 1)->localSdp_.get();
    content.setValue(sdp);
  }

  transports_.at(dialogs_.at(sessionID - 1)->transportID - 1)->sendResponse(response, content);
  qDebug() << "---- Finished sending of a response ---";
}

void SIPTransactions::destroyDialog(uint32_t sessionID)
{
  Q_ASSERT(sessionID != 0 && sessionID <= dialogs_.size());
  if(sessionID == 0 || sessionID > dialogs_.size())
  {
    qCritical() << "ERROR: Bad sessionID for destruction: ";
    return;
  }
  qDebug() << "Destroying dialog:" << sessionID;

  std::shared_ptr<SIPDialogData> dialog = dialogs_.at(sessionID - 1);
  sdp_.endSession(dialog->localSdp_);
  dialog->state.reset();
  dialog->server.reset();
  dialog->client.reset();

  if(dialog->localSdp_)
  {
    sdp_.endSession(dialog->localSdp_);
  }

  dialog->localSdp_.reset();
  dialog->remoteSdp_.reset();
  dialogs_[sessionID - 1].reset();

  // empty all deleted dialogs from end of the list.
  // This does not guarantee that the list wont be filled, but should suffice in most cases.
  while(sessionID == dialogs_.size()
        && sessionID != 0
        && dialogs_.at(sessionID - 1) == nullptr)
  {
    dialogs_.pop_back();
    --sessionID;
  }
}
