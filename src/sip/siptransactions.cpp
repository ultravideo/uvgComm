#include "sip/siptransactions.h"

#include "sip/sipdialog.h"
#include "sip/siptransport.h"

#include "sip/sipclienttransaction.h"
#include "sip/sipservertransaction.h"


SIPTransactions::SIPTransactions():
  isConference_(false),
  sipPort_(5060) // default for SIP, use 5061 for tls encrypted
{}

void SIPTransactions::init(SIPTransactionUser *callControl)
{
  qDebug() << "Iniating SIP Manager";

  transactionUser_ = callControl;

  QObject::connect(&server_, &ConnectionServer::newConnection,
                   this, &SIPTransactions::receiveTCPConnection);

  // listen to everything
  server_.listen(QHostAddress("0.0.0.0"), sipPort_);
  QSettings settings;
  QString username = !settings.value("local/Username").isNull()
      ? settings.value("local/Username").toString() : "anonymous";

  sdp_.setLocalInfo(QHostAddress("0.0.0.0"), username);
  sdp_.setPortRange(21500, 22000, 42);
}

void SIPTransactions::uninit()
{
  //TODO: delete all dialogs
  for(std::shared_ptr<SIPDialogData> dialog : dialogs_)
  {
    destroyDialog(dialog);
  }

  for(std::shared_ptr<SIPTransport> transport : transports_)
  {
    if(transport != NULL)
    {
      transport->destroyConnection();
      transport.reset();
    }
  }
}

QList<uint32_t> SIPTransactions::startCall(QList<Contact> addresses)
{
  QList<uint32_t> calls;
  for(unsigned int i = 0; i < addresses.size(); ++i)
  {
    std::shared_ptr<SIPDialogData> dialogData;
    createDialog(dialogData);

    dialogData->dialog->init(
          SIP_URI{addresses.at(i).username, addresses.at(i).realName, addresses.at(i).remoteAddress});

    dialogData->proxyConnection_ = addresses.at(i).proxyConnection;
    calls.push_back(dialogs_.size());
    // message is sent only after connection has been established so we know our address

    // TODO: we should check if we already have a connection to their adddress.
    if(!addresses.at(i).proxyConnection)
    {
      std::shared_ptr<SIPTransport> transport = createSIPTransport();
      transport->createConnection(TCP, addresses.at(i).remoteAddress);
      dialogData->transportID = transports_.size();
    }
    else
    {
      qWarning() << "ERROR: Proxy connection not implemented yet";
    }

    qDebug() << "Added a new dialog. ID:" << dialogs_.size();

    // this start call will commence once the connection has been established
    if(!dialogData->client->startCall())
    {
      qDebug() << "WARNING: Could not start a call according to session.";
    }
  }

  return calls;
}

void SIPTransactions::createDialog(std::shared_ptr<SIPDialogData>& dialog)
{
  dialog = std::shared_ptr<SIPDialogData> (new SIPDialogData);
  connectionMutex_.lock();
  dialog->dialog = createSIPDialog(dialogs_.size() + 1);
  dialog->client = std::shared_ptr<SIPClientTransaction> (new SIPClientTransaction);
  dialog->client->init(transactionUser_, dialogs_.size() + 1);
  dialog->server = std::shared_ptr<SIPServerTransaction> (new SIPServerTransaction);
  dialog->server->init(transactionUser_, dialogs_.size() + 1);

  QObject::connect(dialog->client.get(), &SIPClientTransaction::sendRequest,
                   this, &SIPTransactions::sendRequest);

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
}

void SIPTransactions::endCall(uint32_t sessionID)
{
  qDebug() << "WARNING: Not implemented in SIP Transactions";
}

void SIPTransactions::endAllCalls()
{
  qDebug() << "WARNING: Not implemented in SIP Transactions";
}

std::shared_ptr<SIPDialog> SIPTransactions::createSIPDialog(uint32_t sessionID)
{
  qDebug() << "Creating SIP Session";
  std::shared_ptr<SIPDialog> dialog = std::shared_ptr<SIPDialog> (new SIPDialog());

  // connect signals to signals. Session is responsible for responses
  // and callmanager handles informing the user.
  return dialog;
}

std::shared_ptr<SIPTransport> SIPTransactions::createSIPTransport()
{
  qDebug() << "Creating SIP transport";
  std::shared_ptr<SIPTransport> connection =
      std::shared_ptr<SIPTransport>(new SIPTransport(transports_.size() + 1));

  QObject::connect(connection.get(), &SIPTransport::sipTransportEstablished,
                   this, &SIPTransactions::connectionEstablished);
  QObject::connect(connection.get(), &SIPTransport::incomingSIPRequest,
                   this, &SIPTransactions::processSIPRequest);
  QObject::connect(connection.get(), &SIPTransport::incomingSIPResponse,
                   this, &SIPTransactions::processSIPResponse);

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

// connection has been established. This enables for us to get the needed info
// to form a SIP message
void SIPTransactions::connectionEstablished(quint32 transportID, QString localAddress,
                                            QString remoteAddress)
{
  Q_ASSERT(transportID != 0 || dialogs_.at(transportID - 1));
  qDebug() << "Establishing connection";

  // TODO: This is not correct and will not work with a prpxy
  qDebug() << "INFO: Connection established will not work with a proxy connection";
  if(transportID == 0 || transportID > dialogs_.size() || dialogs_.at(transportID - 1) == NULL)
  {
    qDebug() << "WARNING: Missing session for connected session";
    return;
  }
  connectionMutex_.lock();
  std::shared_ptr<SIPDialogData> dialog = dialogs_.at(transportID - 1);
  connectionMutex_.unlock();

  if(dialog->dialog == NULL)
  {
    dialog->dialog = createSIPDialog(transportID);
  }

  //dialog->dialog->initDialog(localAddress, SIP_URI(localUsername_, localName_, ));

  dialog->client->connectionReady(true);

  qDebug() << "Connection" << transportID << "connected and dialog created."
           << "From:" << localAddress
           << "To:" << remoteAddress;
}

void SIPTransactions::processSIPRequest(SIPRequest request,
                       quint32 transportID, QVariant content)
{
  qDebug() << "Starting to process received SIP Request:" << request.type;

  // TODO: sessionID is now tranportID
  // TODO: separate nondialog and dialog requests!
  connectionMutex_.lock();

  // find the dialog which corresponds to the callID and tags received in request
  std::shared_ptr<SIPDialogData> foundDialog;

  for(std::shared_ptr<SIPDialogData> dialog : dialogs_)
  {
    if(dialog->dialog->correctRequestDialog(request.message->dialog, request.type, request.message->cSeq))
    {
      foundDialog = dialog;
      break;
    }
  }
  connectionMutex_.unlock();

  if(foundDialog == NULL)
  {
    qDebug() << "Could not find the dialog of the request.";

    if(request.type == INVITE)
    {
      qDebug() << "Someone is trying to start a sip dialog with us!";
      createDialog(foundDialog);

      foundDialog->dialog->init(
            SIP_URI{request.message->from.username,
                    request.message->from.realname,
                    request.message->from.host});

      foundDialog->dialog->processFirstINVITE(request.message);

      // Proxy TODO: somehow distinguish if this is a proxy connection
      foundDialog->proxyConnection_ = false;

      foundDialog->transportID = transportID;
    }
  }

  // check correct initialization
  Q_ASSERT(foundDialog->dialog);

  // TODO: prechecks that the message is ok, then modify program state.

  if(request.type == INVITE)
  {
    if(request.message->content.type == APPLICATION_SDP)
    {
      SDPMessageInfo retrieved = content.value<SDPMessageInfo>();
      foundDialog->localFinalSdp_ = sdp_.localFinalSDP(retrieved);
    }
    else
    {
      qDebug() << "No SDP in INVITE!";
      sendResponse(transportID, SIP_DECLINE, request.type);
      return;
    }
  }

  foundDialog->server->processRequest(request);

  qDebug() << "Finished processing request:" << request.type;
}

void SIPTransactions::processSIPResponse(SIPResponse response,
                                         quint32 transportID, QVariant content)
{
  // TODO: sessionID is now tranportID
  // TODO: separate nondialog and dialog requests!

  connectionMutex_.lock();

  // find the dialog which corresponds to the callID and tags received in response
  std::shared_ptr<SIPDialogData> foundDialog;

  for(std::shared_ptr<SIPDialogData> dialog : dialogs_)
  {
    if(dialog->client->ourResponse(response))
    {
      foundDialog = dialog;
      break;
    }
  }

  if(foundDialog == NULL)
  {
    qDebug() << "Could not find the suggested dialog in response!";
    return;
  }

  // check correct initialization
  Q_ASSERT(foundDialog->dialog);

  connectionMutex_.unlock();

  // TODO: if our request was INVITE and response is 2xx or 101-199, create dialog
  // TODO: prechecks that the response is ok, then modify program state.

  if(response.message->transactionRequest == INVITE && response.type == SIP_OK)
  {
    if(response.message->content.type == APPLICATION_SDP)
    {
      SDPMessageInfo retrieved = content.value<SDPMessageInfo>();
      if(!sdp_.remoteFinalSDP(retrieved))
      {
        qDebug() << "OK SDP not suitable.";
      }
      else
      {
        *foundDialog->remoteFinalSdp_.get() = retrieved;
      }
    }
  }

  qWarning() << "WARNING: Processing responses not implemented yet";

  foundDialog->client->processResponse(response);
}

void SIPTransactions::sendRequest(uint32_t sessionID, RequestType type)
{
  qDebug() << "---- Iniated sending of a request:" << type << "----";
  Q_ASSERT(sessionID != 0 && sessionID <= dialogs_.size());
  // Get all the necessary information from different components.

  std::shared_ptr<SIPTransport> transport
      = transports_.at(dialogs_.at(sessionID - 1)->transportID - 1);
  SIPRequest request;
  request.type = type;

  // if this is the session creation INVITE. Proxy sessions should be created earlier.
  //TODO: Not working for re-INVITE
  if(request.type == INVITE && !dialogs_.at(sessionID - 1)->proxyConnection_)
  {
    dialogs_.at(sessionID - 1)->dialog->createDialog(transport->getLocalAddress());
  }

  // Get message info
  dialogs_.at(sessionID - 1)->client->getRequestMessageInfo(type, request.message);
  dialogs_.at(sessionID - 1)->dialog->getRequestDialogInfo(request.type,
                                                           transport->getLocalAddress(),
                                                           request.message);
  Q_ASSERT(request.message != NULL);
  Q_ASSERT(request.message->dialog != NULL);

  QVariant content;
  request.message->content.length = 0;
  if(type == INVITE)
  {
    request.message->content.type = APPLICATION_SDP;
    SDPMessageInfo sdp = *sdp_.localSDPSuggestion().get();
    content.setValue(sdp);
  }

  transport->sendRequest(request, content);
  qDebug() << "---- Finished sending of a request ---";
}

void SIPTransactions::sendResponse(uint32_t sessionID, ResponseType type, RequestType originalRequest)
{
  qDebug() << "---- Iniated sending of a request:" << type << "----";
  Q_ASSERT(sessionID != 0 && sessionID <= dialogs_.size());

  // Get all the necessary information from different components.
  SIPResponse response;
  response.type = type;
  dialogs_.at(sessionID - 1)->server->getResponseMessage(response.message);
  response.message->transactionRequest = originalRequest;

  QVariant content;
  if(response.message->transactionRequest == INVITE && type == SIP_OK) // TODO: SDP in progress...
  {
    response.message->content.type = APPLICATION_SDP;
    SDPMessageInfo sdp = *dialogs_.at(sessionID - 1)->localFinalSdp_.get();
    content.setValue(sdp);
  }

  transports_.at(dialogs_.at(sessionID - 1)->transportID - 1)->sendResponse(response, content);
  qDebug() << "---- Finished sending of a response ---";
}

void SIPTransactions::destroyDialog(std::shared_ptr<SIPDialogData> dialog)
{
  if(dialog != NULL)
  {
    dialog->dialog.reset();
  }
}
