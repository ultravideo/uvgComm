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
    Connection* con = new Connection;
    connections_.push_back(con);
    QObject::connect(con, SIGNAL(messageAvailable(QString, QString, quint32)),
                     this, SLOT(processMessage(QString, QString, quint32)));

    QString address = addresses.at(i).address.toString();

    con->establishConnection(address, sipPort_);

    std::shared_ptr<CallNegotiation::SIPLink> link = newSIPLink();
    link->connectionID = connections_.size();
    link->peer.address = addresses.at(i).address;

    link->sdp = sdp;
    link->theirTag = "";

    //contact->sender.init(contact->peer.address, sipPort_);

    link->theirName = "Unknown";
    link->theirUsername = "unknown";

    sendRequest(INVITE, link);
  }
}

void CallNegotiation::sendRequest(MessageType request, std::shared_ptr<SIPLink> contact)
{
  // TODO: names
  ++contact->cseq;

  QString branch = generateRandomString(BRANCHLENGTH);

  messageID id = messageComposer_.startSIPString(request);
  messageComposer_.toIP(id, contact->theirName, contact->theirUsername,
                        contact->peer.address, contact->theirTag);
  messageComposer_.fromIP(id, ourName_, ourUsername_, ourLocation_, contact->ourTag);
  messageComposer_.viaIP(id, ourLocation_, branch);
  messageComposer_.maxForwards(id, MAXFORWARDS);
  messageComposer_.setCallID(id, contact->callID);
  messageComposer_.sequenceNum(id, contact->cseq);
  messageComposer_.addSDP(id, contact->sdp);

  QString SIPRequest = messageComposer_.composeMessage(id);

  qDebug() << "Sending the following SIP Request:";
  qDebug().noquote() << SIPRequest;

  QByteArray message = SIPRequest.toUtf8();
  connections_.at(contact->connectionID - 1)->sendPacket(message);
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
    std::unique_ptr<SIPMessageInfo> info = std::move(parseSIPMessage(header));
    qDebug() << "Message parsed";

    if(info != 0)
    {
      Q_ASSERT(info->callID != "");
      if(sessions_.find(info->callID) == sessions_.end())
      {
        qDebug() << "New Call-ID detected:" << info->callID << "Creating session...";

        // Receiving the first message of this SIP connection
        newSIPLinkFromMessage(std::move(info), connectionID);
      }
      else
      {
        qDebug() << "Existing Call-ID detected. Updating session...";

        // updating everything that has changed for our next message
        updateSIPLink(std::move(info));
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

  link->callID.append("@");
  link->callID.append(ourLocation_.toString());

  qDebug() << "Generated CallID: " << link->callID;

  link->cseq = 0;
  link->ourTag = generateRandomString(TAGLENGTH);

  qDebug() << "Generated tag: " << link->ourTag;

  if(sessions_.find(link->callID) == sessions_.end())
  {
    sessions_[link->callID] = link;
  }
  else
  {
    qWarning() << "WARNING: Collision: Call-ID already exists.";
    return 0;
  }

  return link;
}

void CallNegotiation::newSIPLinkFromMessage(std::unique_ptr<SIPMessageInfo> info,
                                            quint32 connectionId)
{
  std::shared_ptr<CallNegotiation::SIPLink> link
      = std::shared_ptr<CallNegotiation::SIPLink> (new SIPLink);


}

void CallNegotiation::updateSIPLink(std::unique_ptr<SIPMessageInfo> info)
{
  qDebug() << "Updating call with ID:" << info->callID;

  std::shared_ptr<SIPLink> oldLink = sessions_[info->callID];

  Q_ASSERT(oldLink && "Old SIP information doesn't exist");

  ++oldLink->cseq;

  if(info->cSeq != oldLink->cseq)
  {
    qDebug() << "Wrong sequence number. Possibly missing SIP messages? Expected cseq:" << oldLink->cseq
             << "got:" << info->cSeq;
  }

}
