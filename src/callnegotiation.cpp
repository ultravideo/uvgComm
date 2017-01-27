#include "callnegotiation.h"

//TODO use cryptographically secure callID generation!!
const QString alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                         "abcdefghijklmnopqrstuvwxyz"
                         "0123456789";

const uint16_t callIDLength = 16;

CallNegotiation::CallNegotiation():
  sessions_(),
  receiver_(),
  messageComposer_(),
  sipPort_(5060) // use 5061 for tls encrypted
{}

CallNegotiation::~CallNegotiation()
{}

void CallNegotiation::init()
{
  qsrand(1);
  ourLocation_ = QHostAddress("127.0.0.1");
}

void CallNegotiation::startCall(QList<Contact> addresses, QString sdp)
{
  ourName_ = "I";
  ourUsername_ = "i";

  qDebug() << "Starting call negotiation";
  for (int i = 0; i < addresses.size(); ++i)
  {
    std::shared_ptr<SIPLink> contact (new SIPLink);
    contact->peer.address = addresses.at(i).address;

    for( unsigned int i = 0; i < callIDLength; ++i )
    {
      contact->callID.append(alphabet.at(qrand()%alphabet.size()));
    }

    contact->callID.append("@");
    contact->callID.append(ourLocation_.toString());


    qDebug() << "Generated CallID: " << contact->callID;

    contact->cseq = 0;

    for( unsigned int i = 0; i < callIDLength; ++i )
    {
      contact->ourTag.append(alphabet.at(qrand()%alphabet.size()));
    }
    qDebug() << "Generated tag: " << contact->ourTag;
    contact->sdp = sdp;
    contact->theirTag = "";

    sessions_[contact->callID] = contact;

    QString branch;

    for( unsigned int i = 0; i < callIDLength; ++i )
    {
      branch.append(alphabet.at(qrand()%alphabet.size()));
    }

    //contact->sender.init(contact->peer.address, sipPort_);

    contact->theirName = "Unknown";
    contact->theirUsername = "unknown";

    sendRequest(INVITE, contact, branch);
  }
}

void CallNegotiation::sendRequest(MessageType request, std::shared_ptr<SIPLink> contact, QString& branch)
{

  // TODO: names
  ++contact->cseq;

  messageID id = messageComposer_.startSIPString(request);
  messageComposer_.toIP(id, contact->theirName, contact->theirUsername, contact->peer.address, contact->theirTag);
  messageComposer_.fromIP(id, ourName_, ourUsername_, ourLocation_, contact->ourTag);
  messageComposer_.viaIP(id, ourLocation_, branch);
  messageComposer_.maxForwards(id, 64);
  messageComposer_.setCallID(id, contact->callID);
  messageComposer_.sequenceNum(id, contact->cseq);
  messageComposer_.addSDP(id, contact->sdp);

  QString SIPRequest = messageComposer_.composeMessage(id);

  qDebug() << "Sending the following SIP Request:";
  qDebug().noquote() << SIPRequest;

  QByteArray message = SIPRequest.toUtf8();

  //contact->sender.sendPacket(message);
}
