#include "callnegotiation.h"

//TODO use cryptographically secure callID generation!!
const QString alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                         "abcdefghijklmnopqrstuvwxyz"
                         "0123456789";

const uint16_t callIDLength = 32;

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
}

void CallNegotiation::startCall(QList<Contact> addresses, QString sdp)
{
  qDebug() << "Starting call negotiation";
  for (int i = 0; i < addresses.size(); ++i)
  {
    std::shared_ptr<SIPLink> contact (new SIPLink);

    contact->peer.address = addresses.at(i).address;

    for( unsigned int i = 0; i < callIDLength; ++i )
    {
      contact->callID.append(alphabet.at(qrand()%alphabet.size()));
    }

    qDebug() << "Generated CallID: " << contact->callID;

    contact->cseq = 0;

    for( unsigned int i = 0; i < callIDLength/2; ++i )
    {
      contact->ourTag.append(alphabet.at(qrand()%alphabet.size()));
    }
    qDebug() << "Generated tag: " << contact->ourTag;
    contact->sdp = sdp;

    contact->theirTag = "";

    sessions_[contact->callID] = contact;

    contact->sender.init(contact->peer.address, sipPort_);

    sendRequest(INVITE, contact);
  }
}

void CallNegotiation::sendRequest(MessageType request, std::shared_ptr<SIPLink> contact)
{
  messageID id = messageComposer_.startSIPString(request);
  // TODO: names
  QHostAddress we = QHostAddress("0.0.0.0");

  messageComposer_.toIP(id, contact->theirName, contact->peer.address, contact->theirTag);
  messageComposer_.fromIP(id, contact->ourName, we, contact->ourTag);

  messageComposer_.viaIP(id, we);

  messageComposer_.maxForwards(id, 64);

  messageComposer_.setCallID(id, contact->callID);

  ++contact->cseq;
  messageComposer_.sequenceNum(id, contact->cseq );

  messageComposer_.addSDP(id, contact->sdp);

  QString SIPRequest = messageComposer_.composeMessage(id);

  qDebug() << "Sending the following SIP Request:" << SIPRequest;

  QByteArray message = SIPRequest.toUtf8();

  contact->sender.sendPacket(message);
}


