#include "callnegotiation.h"

#include "sipparser.h"


//TODO use cryptographically secure callID generation!!
const QString alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                         "abcdefghijklmnopqrstuvwxyz"
                         "0123456789";

const uint16_t CALLIDLENGTH = 16;
const uint16_t TAGLENGTH = 16;
const uint16_t BRANCHLENGTH = 16;

const uint16_t MAXFORWARDS = 70; // the recommmended value is 70



CallNegotiation::CallNegotiation():
  sessions_(),
  messageComposer_(),
  sipPort_(5060), // use 5061 for tls encrypted
  sdpAudioPort_(18888),
  sdpVideoPort_(19888)
{}

CallNegotiation::~CallNegotiation()
{}

void CallNegotiation::init()
{
  qsrand(1);

  foreach (const QHostAddress &address, QNetworkInterface::allAddresses()) {
      if (address.protocol() == QAbstractSocket::IPv4Protocol && address != QHostAddress(QHostAddress::LocalHost))
           qDebug() << address.toString();
  }

  QObject::connect(&server_, SIGNAL(newConnection(Connection*)),
                   this, SLOT(receiveConnection(Connection*)));

  // listen to everything
  // TODO: maybe record the incoming connection address and choose used network interface address
  // according to that
  server_.listen(QHostAddress("0.0.0.0"), sipPort_);

  initUs();
}

void CallNegotiation::uninit()
{}

void CallNegotiation::initUs()
{
  ourName_ = "I";
  ourUsername_ = "i";
}

void CallNegotiation::startCall(QList<Contact> addresses, QString sdp)
{
  Q_ASSERT(addresses.size() != 0);

  qDebug() << "Starting call negotiation";

  for (int i = 0; i < addresses.size(); ++i)
  {
    std::shared_ptr<CallNegotiation::SIPLink> link = newSIPLink();

    Connection* con = new Connection;
    connectionMutex_.lock();
    connections_.push_back(con);
    link->connectionID = connections_.size();
    connectionMutex_.unlock();
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

    link->sdp = sdp;
    link->theirTag = "";

    con->establishConnection(address, sipPort_);

    //contact->sender.init(contact->peer.address, sipPort_);

    // message is sent only after connection has been established so we know our address

    //sendRequest(INVITE, link);
  }
}

void CallNegotiation::acceptCall(QString callID)
{
  if(sessions_.find(callID) == sessions_.end())
  {
    qWarning() << "WARNING: We have lost our link somewhere";
    return;
  }
  sendRequest(OK_200, sessions_[callID], false);
}

void CallNegotiation::rejectCall(QString callID)
{
  if(sessions_.find(callID) == sessions_.end())
  {
    qWarning() << "WARNING: We have lost our link somewhere";
    return;
  }
  sendRequest(DECLINE_600, sessions_[callID], false);
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
    qWarning() << "WARNING: Link not found";
    return;
  }

  foundLink->localAddress = connections_.at(connectionID - 1)->getLocalAddress();
  foundLink->contact.contactAddress
      = connections_.at(connectionID - 1)->getPeerAddress().toString();


  foundLink->host = foundLink->localAddress.toString();

  foundLink->sdp = "v=0 \r\n"
                   "o=" + ourUsername_ + " 1234 12345 IN IP4 " + foundLink->localAddress.toString() + "\r\n"
                   "s=HEVC Video Conference\r\n"
                   "t=0 0\r\n"
                   "c=IN IP4 " + foundLink->localAddress.toString() + "\r\n"
                   "m=video " + QString::number(sdpVideoPort_) + " RTP/AVP 97\r\n"
                   "m=audio " + QString::number(sdpAudioPort_) + " RTP/AVP 96\r\n";

  sdpAudioPort_ += 2;
  sdpVideoPort_ += 2;
  sendRequest(INVITE, foundLink, true);
}

void CallNegotiation::sendRequest(MessageType request, std::shared_ptr<SIPLink> link, bool isRequest)
{
  // TODO: names
  ++link->cSeq;

  if(isRequest)
  {
    link->originalRequest = request;
  }

  QString branch = generateRandomString(BRANCHLENGTH);

  messageID id = messageComposer_.startSIPString(request);
  messageComposer_.to(id, link->contact.name, link->contact.username,
                        link->contact.contactAddress, link->theirTag);
  messageComposer_.fromIP(id, ourName_, ourUsername_, link->localAddress, link->ourTag);
  messageComposer_.viaIP(id, link->localAddress, branch);
  messageComposer_.maxForwards(id, MAXFORWARDS);
  messageComposer_.setCallID(id, link->callID, link->host);
  messageComposer_.sequenceNum(id, link->cSeq, link->originalRequest);
  messageComposer_.addSDP(id, link->sdp);

  QString SIPRequest = messageComposer_.composeMessage(id);

  qDebug() << "Sending the following SIP Request:";
  qDebug().noquote() << SIPRequest;

  QByteArray message = SIPRequest.toUtf8();
  connections_.at(link->connectionID - 1)->sendPacket(message);
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
    std::shared_ptr<SDPMessageInfo> sdpInfo = NULL;
    if(info->contentType == "application/sdp" && content.size() != 0)
    {
      sdpInfo = parseSDPMessage(content);
      if(sdpInfo == NULL)
      {
        qDebug() << "SDP parsing failed";
      }
    }

    qDebug() << "Message parsed";

    if(info != 0)
    {
      Q_ASSERT(info->callID != "");

      if(compareSIPLinkInfo(info, connectionID))
      {
        newSIPLinkFromMessage(info, connectionID);
        switch(info->request)
        {
          case INVITE:
          {
            qDebug() << "Found INVITE";

            if(sdpInfo == NULL)
            {
              qDebug() << "No SDP received in INVITE";
              return;
            }
            if(sessions_[info->callID]->contact.contactAddress
               == sessions_[info->callID]->localAddress.toString())
            {
              emit callingOurselves(sdpInfo);
            }
            else
            {
              emit incomingINVITE(info->callID, sessions_[info->callID]->contact.name);
            }
            break;
          }
          case ACK:
          {
            qDebug() << "Found ACK";

            emit callNegotiated(sdpInfo);
            break;
          }
          case OK_200:
          {
            qDebug() << "Found 200 OK";
            if(sessions_[info->callID]->originalRequest == INVITE)
            {
              if(sdpInfo == NULL)
              {
                qDebug() << "No SDP received in INVITE OK reponse";
                return;
              }
              emit ourCallAccepted(info->callID, sdpInfo);
              sendRequest(ACK, sessions_[info->callID], false);
            }
            else
            {
              qDebug() << "Received a response for unrecognized request";
            }
            break;
          }
          default:
          {
            qDebug() << "Warning. Rrequest/response unimplemented";
          }
        }
      }
      else
      {
        // TODO: send 400
        qDebug() << "Problem detected in SIP message based on previous information!!";
      }
    }
    else
    {
      qDebug() << "Invalid SIP message received";
    }
  }
}

QString CallNegotiation::generateRandomString(uint32_t length)
{
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

  link->cSeq = 0;
  link->ourTag = generateRandomString(TAGLENGTH);

  qDebug() << "Generated tag: " << link->ourTag;

  if(sessions_.find(link->callID) == sessions_.end())
  {
    sessions_[link->callID] = link;
    qDebug() << "SIPLink added to sessions";
  }
  else
  {
    qWarning() << "WARNING: Collision: Call-ID already exists.";
    return 0;
  }
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
  link->contact = {info->theirName, info->theirUsername, info->contactAddress};
  link->replyAddress = info->ourLocation;
  link->localAddress = info->ourLocation;
  link->host = info->host;

  link->cSeq = info->cSeq;

  link->ourTag = generateRandomString(TAGLENGTH);
  link->theirTag = info->theirTag;

  //TODO evaluate SDP

  link->connectionID = connectionID;
  link->originalRequest = info->originalRequest;

  sessions_[info->callID] = link;
}

bool CallNegotiation::compareSIPLinkInfo(std::shared_ptr<SIPMessageInfo> info,
                                         quint32 connectionID)
{
  qDebug() << "Checking SIP message by comparing it to existing information.";

  // if we don't have previous information
  if(sessions_.find(info->callID) == sessions_.end())
  {
    qDebug() << "New Call-ID detected:" << info->callID << "Creating session...";

    if(info->ourTag != "" && STRICTSIPPROCESSING)
    {
      qDebug() << "WHY DO THEY HAVE OUR TAG BEFORE IT WAS GENERATED!?!";
      return false;
    }

    if(info->cSeq != 1 && STRICTSIPPROCESSING)
    {
      qDebug() << "Weird first cseq:" << info->cSeq;
      return false;
    }
  }
  // if this is not the first message of this call
  else
  {
    qDebug() << "Existing Call-ID detected.";
    std::shared_ptr<SIPLink> link = sessions_[info->callID];

    // this would be our problem
    Q_ASSERT(link->callID == info->callID);

    if(!info->ourTag.isEmpty() && info->ourTag != link->ourTag &&
       STRICTSIPPROCESSING)
    {
      qDebug() << "Our tag has changed! Something went wrong on other end.";
      return false;
    }

    if(!link->theirTag.isEmpty() && info->theirTag != link->theirTag &&
       STRICTSIPPROCESSING)
    {
      qDebug() << "They have changed their tag! Something went wrong.";
      return false;
    }

    if(info->cSeq != link->cSeq + 1)
    {
      qDebug() << "We are missing a message!. Expected cseq:" << link->cSeq + 1
               << "Got:" << info->cSeq;
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
  if(info->ourLocation != connections_.at(connectionID - 1)->getLocalAddress().toString())
  {
    qDebug() << "We are not connected to their address:" << info->ourLocation;
    return false;
  }
  if(info->theirLocation != connections_.at(connectionID - 1)->getPeerAddress().toString())
  {
    qDebug() << "We are not connected to their address:" << info->theirLocation;
    return false;
  }
  connectionMutex_.unlock();

  return true;
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
