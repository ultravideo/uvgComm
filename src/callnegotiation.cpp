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
  sipPort_(5060) // use 5061 for tls encrypted
{}

CallNegotiation::~CallNegotiation()
{}

void CallNegotiation::init()
{
  qsrand(1);
  ourLocation_ = QHostAddress("127.0.0.1");

  QObject::connect(&server_, SIGNAL(newConnection(Connection*)),
                   this, SLOT(receiveConnection(Connection*)));

  server_.listen(ourLocation_, sipPort_);

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

    QString address = addresses.at(i).contactAddress;
    con->establishConnection(address, sipPort_);

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

    //contact->sender.init(contact->peer.address, sipPort_);

    sendRequest(INVITE, link);
  }
}

void CallNegotiation::sendRequest(MessageType request, std::shared_ptr<SIPLink> link)
{
  // TODO: names
  ++link->cSeq;

  QString branch = generateRandomString(BRANCHLENGTH);

  messageID id = messageComposer_.startSIPString(request);
  messageComposer_.to(id, link->contact.name, link->contact.username,
                        link->contact.contactAddress, link->theirTag);
  messageComposer_.fromIP(id, ourName_, ourUsername_, ourLocation_, link->ourTag);
  messageComposer_.viaIP(id, ourLocation_, branch);
  messageComposer_.maxForwards(id, MAXFORWARDS);
  messageComposer_.setCallID(id, link->callID, link->host);
  messageComposer_.sequenceNum(id, link->cSeq);
  messageComposer_.addSDP(id, link->sdp);

  QString SIPRequest = messageComposer_.composeMessage(id);

  qDebug() << "Sending the following SIP Request:";
  qDebug().noquote() << SIPRequest;

  QByteArray message = SIPRequest.toUtf8();
  connections_.at(link->connectionID - 1)->sendPacket(message);

  link->sentRequest = request;
}

void CallNegotiation::receiveConnection(Connection* con)
{
  QObject::connect(con, SIGNAL(messageAvailable(QString,QString, quint32)),
                   this, SLOT(processMessage(QString, QString, quint32)));
  connections_.push_back(con);

  con->setID(connections_.size());
}

void CallNegotiation::processMessage(QString header, QString content, quint32 connectionID)
{
  qDebug() << "Processing message";

  if(connectionID != 0)
  {
    std::shared_ptr<SIPMessageInfo> info = std::move(parseSIPMessage(header));
    qDebug() << "Message parsed";

    if(info != 0)
    {
      Q_ASSERT(info->callID != "");

      if(compareSIPLinkInfo(info, connectionID))
      {
        QString callID = info->callID;
        MessageType type = info->request;

        newSIPLinkFromMessage(info, connectionID);
        switch(type)
        {
          case INVITE:
          {
            qDebug() << "Found INVITE";
            emit incomingINVITE(callID, sessions_[callID]->contact.name);
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
  link->host = ourLocation_.toString();

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
  std::shared_ptr<CallNegotiation::SIPLink> link
      = std::shared_ptr<CallNegotiation::SIPLink> (new SIPLink);

  link->callID = info->callID;
  link->contact = {info->theirName, info->theirUsername, info->contactAddress};
  link->replyAddress = info->replyAddress;

  link->cSeq = info->cSeq;

  link->ourTag = generateRandomString(TAGLENGTH);

  link->theirTag = info->theirTag;

  //TODO evaluate SDP

  link->connectionID = connectionID;
  link->sentRequest = NOREQUEST;

  sessions_[info->callID] = link;
}

bool CallNegotiation::compareSIPLinkInfo(std::shared_ptr<SIPMessageInfo> info, quint32 connectionID)
{

  qDebug() << "Checking SIP message by comparing it to existing information.";

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
  else
  {
    qDebug() << "Existing Call-ID detected.";
    std::shared_ptr<SIPLink> link = sessions_[info->callID];

    // this would be our problem
    Q_ASSERT(link->callID == info->callID);

    if(!info->ourTag.isEmpty() && info->ourTag != link->ourTag && STRICTSIPPROCESSING)
    {
      qDebug() << "Our tag has changed! Something went wrong on other end.";
      return false;
    }

    if(!link->theirTag.isEmpty() && info->theirTag != link->theirTag && STRICTSIPPROCESSING)
    {
      qDebug() << "They have changed their tag! Something went wrong.";
      return false;
    }

    if(info->cSeq != link->cSeq + 1)
    {
      qDebug() << "We are missing a message!. Expected cseq:" << link->cSeq + 1
               << "Got:" << info->cSeq;
    }

    if(link->connectionID != connectionID)
    {
      qWarning() << "WARNING: Unexpected change of connection ID";
      qDebug() << "Maybe we are calling ourselves";
    }

    // TODO  Maybe check the name portion also
  }

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
