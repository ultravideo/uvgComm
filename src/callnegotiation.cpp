#include "callnegotiation.h"

#include "connection.h"
#include "sipparser.h"

//TODO use cryptographically secure callID generation!!
const QString alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                         "abcdefghijklmnopqrstuvwxyz"
                         "0123456789";

const uint16_t CALLIDLENGTH = 16;
const uint16_t TAGLENGTH = 16;
const uint16_t BRANCHLENGTH = 16;
const uint16_t MAXFORWARDS = 70; // the recommmended value is 70
const uint16_t STARTPORT = 18888;

CallNegotiation::CallNegotiation():
  sessions_(),
  messageComposer_(),
  sipPort_(5060), // default for sip, use 5061 for tls encrypted
  localName_("Anonymous"),
  firstAvailablePort_(STARTPORT)
{}

CallNegotiation::~CallNegotiation()
{}

void CallNegotiation::init(QString localName, QString localUsername)
{
  qsrand(1);

  QObject::connect(&server_, SIGNAL(newConnection(Connection*)),
                   this, SLOT(receiveConnection(Connection*)));

  // listen to everything
  // TODO: maybe record the incoming connection address and choose used network interface address
  // according to that
  server_.listen(QHostAddress("0.0.0.0"), sipPort_);

  if(!localName.isEmpty())
  {
    localName_ = localName;
  }
  localUsername_ = localUsername;
}

void CallNegotiation::uninit()
{
  for (uint16_t connectionID = 1; connectionID <= connections_.size();
       ++connectionID)
  {
    stopConnection(connectionID);
  }
  connections_.empty();
  sessions_.clear();
}

std::shared_ptr<SDPMessageInfo> CallNegotiation::generateSDP(QString localAddress)
{
  // TODO: Get suitable SDP from media manager
  QString sdp_str = "v=0 \r\n"
                   "o=" + localUsername_ + " 1234 12345 IN IP4 " + localAddress + "\r\n"
                   "s=HEVC Video Conference\r\n"
                   "t=0 0\r\n"
                   "c=IN IP4 " + localAddress + "\r\n"
                   "m=video " + QString::number(firstAvailablePort_) + " RTP/AVP 97\r\n"
                   "m=audio " + QString::number(firstAvailablePort_ + 2) + " RTP/AVP 96\r\n";

  firstAvailablePort_ += 4; // video and audio + rtcp for both

  std::shared_ptr<SDPMessageInfo> sdp = parseSDPMessage(sdp_str);

  return sdp;
}

void CallNegotiation::endCall(QString callID)
{
  sessionMutex_.lock();
  if(sessions_.find(callID) == sessions_.end())
  {
    qWarning() << "WARNING: Ending a call that doesn't exist";
    return;
  }

  sendRequest(BYE, sessions_[callID]);

  uninitSession(sessions_[callID]);
  sessionMutex_.lock();
}

QList<QString> CallNegotiation::startCall(QList<Contact> addresses)
{
  Q_ASSERT(addresses.size() != 0);

  QList<QString> callIDs;
  qDebug() << "Starting call negotiation with " << addresses.size() << "addresses";

  // TODO create a conference with these participants
  for (int i = 0; i < addresses.size(); ++i)
  {
    qDebug() << "Creating call number:" << i;
    std::shared_ptr<CallNegotiation::SIPLink> link = newSIPLink();
    Connection* con = new Connection(connections_.size() + 1);
    connectionMutex_.lock();
    connections_.push_back(con);
    link->connectionID = con->getID();
    connectionMutex_.unlock();

    qDebug() << "Creating connection with ID:" << link->connectionID;

    QObject::connect(con, SIGNAL(messageAvailable(QString, QString, quint32)),
                     this, SLOT(processMessage(QString, QString, quint32)));

    QObject::connect(con, SIGNAL(connected(quint32)),
                     this, SLOT(connectionEstablished(quint32)));

    QString address = addresses.at(i).contactAddress;

    link->contact = addresses.at(i);

    if(link->contact.name.isEmpty())
    {
      qWarning() << "Warning: No name was given for callee";
      link->contact.name = "Unknown";
    }

    if(link->contact.username.isEmpty())
    {
      qWarning() << "Warning: No username was given for callee";
      link->contact.username = "unknown";
    }

    link->remoteTag = "";

    con->establishConnection(address, sipPort_);
    // message is sent only after connection has been established so we know our address

    callIDs.append(link->callID);
  }
  return callIDs;
}

void CallNegotiation::acceptCall(QString callID)
{
  if(sessions_.find(callID) == sessions_.end())
  {
    qWarning() << "WARNING: We have lost our link somewhere";
    return;
  }
  sendResponse(OK_200, sessions_[callID]);
}

void CallNegotiation::rejectCall(QString callID)
{
  if(sessions_.find(callID) == sessions_.end())
  {
    qWarning() << "WARNING: We have lost our link somewhere";
    return;
  }
  sendResponse(DECLINE_603, sessions_[callID]);
}

void CallNegotiation::connectionEstablished(quint32 connectionID)
{
  qDebug() << "Connection established for id:" << connectionID;

  std::shared_ptr<SIPLink> foundLink;
  for(auto link : sessions_)
  {
    if(link.second->connectionID == connectionID)
    {
      qDebug() << "link found";
      foundLink = link.second;
    }
  }

  Q_ASSERT(foundLink);
  if(!foundLink)
  {
    qWarning() << "WARNING: Link not found for established session. ID:" << connectionID;
    for(auto link : sessions_)
    {
      qDebug() << "Existing IDs:" << link.second->connectionID;
    }
    return;
  }

  // update SIP fields for future communications. Needed because our address may vary
  // based on which interface the their address is accessed through
  foundLink->localAddress = connections_.at(connectionID - 1)->getLocalAddress();
  foundLink->contact.contactAddress
      = connections_.at(connectionID - 1)->getPeerAddress().toString();

  foundLink->host = foundLink->localAddress.toString();
  foundLink->localSDP = generateSDP(foundLink->localAddress.toString());

  sendRequest(INVITE, foundLink);
}

void CallNegotiation::messageComposition(messageID id, std::shared_ptr<SIPLink> link)
{

  messageComposer_.to(id, link->contact.name, link->contact.username,
                        link->contact.contactAddress, link->remoteTag);
  messageComposer_.fromIP(id, localName_, localUsername_, link->localAddress, link->localTag);
  QString branch = generateRandomString(BRANCHLENGTH);
  messageComposer_.viaIP(id, link->localAddress, branch);
  messageComposer_.maxForwards(id, MAXFORWARDS);
  messageComposer_.setCallID(id, link->callID, link->host);
  messageComposer_.sequenceNum(id, 1, link->originalRequest);

  QString SIPRequest = messageComposer_.composeMessage(id);

  qDebug() << "Sending the following SIP Request:";
  qDebug() << SIPRequest;

  QByteArray message = SIPRequest.toUtf8();
  if(connections_.at(link->connectionID - 1) != 0)
  {
    connections_.at(link->connectionID - 1)->sendPacket(message);
  }
  else
  {
    qWarning() << "WARNING: Tried to send message with destroyed connection!";
  }
}

void CallNegotiation::sendRequest(RequestType request, std::shared_ptr<SIPLink> link)
{
  link->originalRequest = request;
  messageID id = messageComposer_.startSIPRequest(request);

  if(request == INVITE)
  {
    QString sdp_str = messageComposer_.formSDP(link->localSDP);
    messageComposer_.addSDP(id, sdp_str);
  }

  messageComposition(id, link);
}

void CallNegotiation::sendResponse(ResponseType response, std::shared_ptr<SIPLink> link)
{
  messageID id = messageComposer_.startSIPResponse(response);

  if(link->originalRequest == INVITE && response == OK_200)
  {
    QString sdp_str = messageComposer_.formSDP(link->localSDP);
    messageComposer_.addSDP(id, sdp_str);
  }

  messageComposition(id, link);
}

void CallNegotiation::receiveConnection(Connection* con)
{
  QObject::connect(con, SIGNAL(messageAvailable(QString,QString, quint32)),
                   this, SLOT(processMessage(QString, QString, quint32)));
  connections_.push_back(con);

  con->setID(connections_.size());
}

void CallNegotiation::processMessage(QString header, QString content,
                                     quint32 connectionID)
{
  qDebug() << "Processing message";

  if(connectionID != 0)
  {
    std::shared_ptr<SIPMessageInfo> info = parseSIPMessage(header);
    if(info == NULL)
    {
      qDebug() << "Failed to parse SIP message";
      //sendResponse(MALFORMED_400, ); TODO: get link and attempted request from somewhere
      return;
    }

    std::shared_ptr<SDPMessageInfo> sdpInfo = NULL;
    if(info->contentType == "application/sdp" && content.size() != 0)
    {
      sdpInfo = parseSDPMessage(content);
      if(sdpInfo == NULL)
      {
        qDebug() << "SDP parsing failed";
        return;
        //sendResponse(MALFORMED_400, );
      }
    }

    qDebug() << "Message parsed";

    if(info != 0)
    {
      Q_ASSERT(info->callID != "");
      sessionMutex_.lock();
      if(compareSIPLinkInfo(info, connectionID))
      {
        newSIPLinkFromMessage(info, connectionID);

        if(info->request != NOREQUEST && info->response == NORESPONSE)
        {
          processRequest(info, sdpInfo, connectionID);
        }
        else if(info->request == NOREQUEST && info->response != NORESPONSE)
        {
          processResponse(info, sdpInfo);
        }
        else
        {
          qWarning() << "WARNING: No response or request indicated in parsing";
        }
      }
      else
      {
        // TODO: send 400
        qDebug() << "Problem detected in SIP message based on previous information!!";
      }
      sessionMutex_.unlock();
    }
    else
    {
      qDebug() << "Invalid SIP message received";
    }
  }
}

QString CallNegotiation::generateRandomString(uint32_t length)
{
  // TODO make this cryptographically secure
  QString string;
  for( unsigned int i = 0; i < length; ++i )
  {
    string.append(alphabet.at(qrand()%alphabet.size()));
  }
  return string;
}

std::shared_ptr<CallNegotiation::SIPLink> CallNegotiation::newSIPLink()
{
  std::shared_ptr<SIPLink> link (new SIPLink);

  link->callID = generateRandomString(CALLIDLENGTH);

  qDebug() << "Generated CallID: " << link->callID;

  link->localTag = generateRandomString(TAGLENGTH);

  qDebug() << "Generated tag: " << link->localTag;

  sessionMutex_.lock();
  if(sessions_.find(link->callID) == sessions_.end())
  {
    sessions_[link->callID] = link;
    qDebug() << "SIPLink added to sessions";
  }
  else
  {
    qWarning() << "WARNING: Collision: Call-ID already exists.";
    sessionMutex_.unlock();
    return 0;
  }
  sessionMutex_.unlock();
  return link;
}

void CallNegotiation::newSIPLinkFromMessage(std::shared_ptr<SIPMessageInfo> info,
                                            quint32 connectionID)
{
  std::shared_ptr<CallNegotiation::SIPLink> link;

  if(sessions_.find(info->callID) != sessions_.end())
  {
    link = sessions_[info->callID];
  }
  else
  {
    link = std::shared_ptr<CallNegotiation::SIPLink> (new SIPLink);
  }

  link->callID = info->callID;
  link->contact = {info->remoteName, info->remoteUsername, info->contactAddress};
  link->replyAddress = info->localLocation;
  link->localAddress = info->localLocation;
  link->host = info->host;

  if(!info->localTag.isEmpty())
  {
    link->localTag = info->localTag;
  }
  else
  {
    link->localTag = generateRandomString(TAGLENGTH);
  }
  link->remoteTag = info->remoteTag;

  //TODO evaluate SDP

  link->connectionID = connectionID;
  link->originalRequest = info->originalRequest;

  if(link->localSDP == NULL)
  {
    link->localSDP = generateSDP(info->localLocation);
  }

  // TODO: make user the sdp is checked somewhere.
  sessions_[info->callID] = link;
}

bool CallNegotiation::compareSIPLinkInfo(std::shared_ptr<SIPMessageInfo> info,
                                         quint32 connectionID)
{
  qDebug() << "Checking SIP message by comparing it to existing information.";

  if(connectionID > connections_.size() || connections_.at(connectionID - 1) == 0)
  {
    qWarning() << "WARNING: Bad connection ID:"
               << connectionID << "/" << connections_.size();
    return false;
  }

  // TODO: This relies on outside information.
  // Maybe keep the SIPLink until our BYE has been received?
  if(info->localLocation == info->remoteLocation)
  {
    qDebug() << "It is us. Lets forget checking the message";
    return true;
  }

  // if we don't have previous information
  if(sessions_.find(info->callID) == sessions_.end())
  {
    qDebug() << "New Call-ID detected:" << info->callID << "Creating session...";

    if(info->localTag != "" && STRICTSIPPROCESSING)
    {
      qDebug() << "WHY DO THEY HAVE OUR TAG BEFORE IT WAS GENERATED!?!";
      return false;
    }

    if(info->response != NORESPONSE)
    {
      qDebug() << "Got a response as a first messsage. Discarding.";
      return false;
    }

    if(info->request == BYE // No call to end
       || info->request == ACK // No INVITE response to confirm
       || info->request == CANCEL // No request to be cancelled
       || info->request == NOREQUEST)
    {
      qDebug() << "Got an unsuitable request as first request:" << info->request;
      return false;
    }

    if(info->cSeq != 1)
    {
      qDebug() << "Not our application based on first:" << info->cSeq;
    }
  }
  // if this is not the first message of this call
  else
  {
    qDebug() << "Existing Call-ID detected.";
    std::shared_ptr<SIPLink> link = sessions_[info->callID];

    // this would be our problem
    Q_ASSERT(link->callID == info->callID);

    if(!info->localTag.isEmpty() && info->localTag != link->localTag &&
       STRICTSIPPROCESSING)
    {
      qDebug() << "Our tag has changed! Something went wrong on other end.";
      return false;
    }

    if(!link->remoteTag.isEmpty() && info->remoteTag != link->remoteTag &&
       STRICTSIPPROCESSING)
    {
      qDebug() << "They have changed their tag! Something went wrong. Old:" << link->remoteTag
               << "new:" << info->remoteTag;
      return false;
    }

    if(info->cSeq != 1)
    {
      qDebug() << "CSeq differs from 1 Got:" << info->cSeq;
    }

    if(link->host != info->host)
    {
      qDebug() << "They changed the host!!";
      return false;
    }

    if(link->connectionID != connectionID)
    {
      qWarning() << "WARNING: Unexpected change of connection ID";
      qDebug() << "Maybe we are calling ourselves";
    }

    // TODO  Maybe check the name portion also
  }

  // check connection details
  connectionMutex_.lock();
  if(connections_.at(connectionID - 1)->connected())
  {
    if(info->localLocation != connections_.at(connectionID - 1)->getLocalAddress().toString())
    {
      qDebug() << "We are not connected from our address:" << info->localLocation;
      return false;
    }
    if(info->remoteLocation != connections_.at(connectionID - 1)->getPeerAddress().toString())
    {
      qDebug() << "We are not connected to their address:" << info->remoteLocation;
      return false;
    }
  }
  connectionMutex_.unlock();

  return true;
}

void CallNegotiation::processRequest(std::shared_ptr<SIPMessageInfo> info,
                                     std::shared_ptr<SDPMessageInfo> peerSDP,
                                     quint32 connectionID)
{
  switch(info->request)
  {
    case INVITE:
    {
      qDebug() << "Found INVITE";

      if(peerSDP == NULL)
      {
        qDebug() << "No SDP received in INVITE";
        // TODO: send malformed request
        return;
      }
      if(sessions_[info->callID]->contact.contactAddress
         == sessions_[info->callID]->localAddress.toString())
      {
        emit callingOurselves(info->callID, peerSDP);
      }
      else
      {
        if(peerSDP)
        {
          if(suitableSDP(peerSDP))
          {
            sessions_[info->callID]->peerSDP = peerSDP;
            emit incomingINVITE(info->callID, sessions_[info->callID]->contact.name);
          }
          else
          {
            qDebug() << "Could not find suitable SDP parameters within INVITE. Terminating..";
            // TODO implement this response
            sendResponse(UNSUPPORTED_413, sessions_[info->callID]);
          }
        }
        else
        {
          qDebug() << "No sdpInfo received in request!";
          sendResponse(MALFORMED_400, sessions_[info->callID]);
        }
      }
      break;
    }
    case ACK:
    {
      qDebug() << "Found ACK";
      if(sessions_[info->callID]->peerSDP)
      {
        emit callNegotiated(info->callID, sessions_[info->callID]->peerSDP,
            sessions_[info->callID]->localSDP);
      }
      else
      {
        qDebug() << "Got ACK without a previous SDP";
      }
      break;
    }
    case BYE:
    {
      qDebug() << "Found BYE";

      sendResponse(OK_200, sessions_[info->callID]);
      emit callEnded(info->callID, info->replyAddress);

      if(connectionID != sessions_[info->callID]->connectionID)
      {
        stopConnection(connectionID);
      }
      uninitSession(sessions_[info->callID]);

      break;
    }
    default:
    {
      qWarning() << "WARNING: Request not implemented";
      break;
    }
  }
}

void CallNegotiation::processResponse(std::shared_ptr<SIPMessageInfo> info,
                                      std::shared_ptr<SDPMessageInfo> peerSDP)
{
  switch(info->response)
  {
    case OK_200:
    {
      qDebug() << "Found 200 OK";
      if(sessions_[info->callID]->originalRequest == INVITE)
      {
        if(peerSDP)
        {
          if(suitableSDP(peerSDP))
          {
            sessions_[info->callID]->peerSDP = peerSDP;
            emit ourCallAccepted(info->callID, sessions_[info->callID]->peerSDP,
                sessions_[info->callID]->localSDP);
            sendRequest(ACK, sessions_[info->callID]);
          }
          else
          {
            qDebug() << "SDP not supported";
            // TODO implement this response
            sendResponse(UNSUPPORTED_413, sessions_[info->callID]);
          }
        }
        else
        {
          qDebug() << "No sdpInfo received in INVITE OK response!";
          sendResponse(MALFORMED_400, sessions_[info->callID]);
        }

      }
      else if(sessions_[info->callID]->originalRequest == BYE)
      {
        qDebug() << "They accepted our BYE";
      }
      else if(sessions_[info->callID]->originalRequest != NOREQUEST)
      {
        qWarning() << "WARNING: Response processing not implemented for this request:"
                   << sessions_[info->callID]->originalRequest;
      }
      else
      {
        qDebug() << "Received a response for unrecognized request";
      }
      break;
    }
    case DECLINE_603:
    {
      if(sessions_[info->callID]->originalRequest == INVITE)
      {
        qDebug() << "Received DECLINE";
        emit ourCallRejected(info->callID);
      }
      else
      {
        qDebug() << "Unimplemented decline for:" << sessions_[info->callID]->originalRequest;
      }
      break;
    }
    default:
    {
      qWarning() << "WARNING: Response processing not implemented!";
    }
  }
}

bool CallNegotiation::suitableSDP(std::shared_ptr<SDPMessageInfo> peerSDP)
{
  Q_ASSERT(peerSDP);
  if(peerSDP == NULL)
  {
    return false;
  }

  // TODO check codec

  //checks if the stream has both audio and video. TODO: Both are actually not necessary
  bool audio = false;
  bool video = false;

  for(auto media : peerSDP->media)
  {
    if(media.type == "video")
    {
      video = true;
    }
    if(media.type == "audio")
    {
      audio = true;
    }
  }

  return audio && video;
}

void CallNegotiation::endAllCalls()
{
  sessionMutex_.lock();
  for(auto session : sessions_)
  {
    sendRequest(BYE, session.second);
  }

  while(!sessions_.empty())
  {
    uninitSession((*sessions_.begin()).second);
  }
  sessionMutex_.unlock();
}

void CallNegotiation::stopConnection(quint32 connectionID)
{
  if(connectionID != 0
     && connections_.at(connectionID - 1) != 0
     && connections_.size() > connectionID - 1 )
  {
    connections_.at(connectionID - 1)->exit(0); // stops qthread
    connections_.at(connectionID - 1)->stopConnection(); // exits run loop
    while(connections_.at(connectionID - 1)->isRunning())
    {
      qSleep(5);
    }
    delete connections_.at(connectionID - 1);
    connections_.at(connectionID - 1) = 0;
  }
}

void CallNegotiation::uninitSession(std::shared_ptr<SIPLink> link)
{
  stopConnection(link->connectionID);
  sessions_.erase(link->callID);
}

QList<QHostAddress> parseIPAddress(QString address)
{
  QList<QHostAddress> ipAddresses;

  QRegularExpression re("\\b((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)(\\.|$)){4}\\b");
  if(re.match(address).hasMatch())
  {
    qDebug() << "Found IPv4 address:" << address;
    ipAddresses.append(QHostAddress(address));
  }
  else
  {
    qDebug() << "Did not find IPv4 address:" << address;
    QHostInfo hostInfo;
    hostInfo.setHostName(address);

    ipAddresses.append(hostInfo.addresses());
  }
  return ipAddresses;
}

