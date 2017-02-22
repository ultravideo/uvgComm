#include "callnegotiation.h"

#include "sipparser.h"


//TODO use cryptographically secure callID generation!!
const QString alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                         "abcdefghijklmnopqrstuvwxyz"
                         "0123456789";

const uint16_t callIDLength = 16;

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

  QObject::connect(&server_, SIGNAL(newConnection(Connection*)), this, SLOT(receiveConnection(Connection*)));

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

std::shared_ptr<CallNegotiation::SIPLink> CallNegotiation::createNewsSIPLink()
{
  std::shared_ptr<SIPLink> link (new SIPLink);

  link->callID = generateRandomString(callIDLength);

  link->callID.append("@");
  link->callID.append(ourLocation_.toString());

  qDebug() << "Generated CallID: " << link->callID;

  link->cseq = 0;
  link->ourTag = generateRandomString(callIDLength);

  qDebug() << "Generated tag: " << link->ourTag;

  sessions_[link->callID] = link;

  return link;
}

void CallNegotiation::startCall(QList<Contact> addresses, QString sdp)
{
  qDebug() << "Starting call negotiation";
  for (int i = 0; i < addresses.size(); ++i)
  {
    Connection* con = new Connection;
    connections_.push_back(con);
    QObject::connect(con, SIGNAL(messageAvailable(QString, QString, uint32_t)),
                     this, SLOT(processMessage(QString, QString, uint32_t)));

    QString address = addresses.at(i).address.toString();

    con->establishConnection(address, sipPort_);

    std::shared_ptr<CallNegotiation::SIPLink> link = createNewsSIPLink();
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

  QString branch = generateRandomString(callIDLength);

  messageID id = messageComposer_.startSIPString(request);
  messageComposer_.toIP(id, contact->theirName, contact->theirUsername, contact->peer.address, contact->theirTag);
  messageComposer_.fromIP(id, ourName_, ourUsername_, ourLocation_, contact->ourTag);
  messageComposer_.viaIP(id, ourLocation_, branch);
  messageComposer_.maxForwards(id, 70); // 70 is the recommended value
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
  QObject::connect(con, SIGNAL(messageAvailable(QString,QString,uint32_t)), this, SLOT(processMessage(QString, QString, uint32_t)));
  connections_.push_back(con);

  con->setID(connections_.size());
}

void CallNegotiation::processMessage(QString header, QString content, uint32_t connectionID)
{
  if(connectionID != 0)
  {
    std::unique_ptr<SIPMessageInfo> info = std::move(parseSIPMessage(header));

    if(info != 0)
    {
      Q_ASSERT(info->callID != "");
      if(sessions_.find(info->callID) == sessions_.end())
      {
        //sessions_[info->callID] = ;
      }
      else
      {

      }
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
