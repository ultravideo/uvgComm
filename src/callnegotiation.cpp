#include "callnegotiation.h"


//TODO use cryptographically secure callID generation!!

const QString alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                         "abcdefghijklmnopqrstuvwxyz"
                         "0123456789";

const uint16_t callIDLength = 32;

CallNegotiation::CallNegotiation()
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
    std::unique_ptr<SIPLink> contact (new SIPLink);

    contact->peer.address = addresses.at(i).address;

    for( unsigned int i = 0; i < callIDLength; ++i )
    {
      contact->callID.append(alphabet.at(qrand()%alphabet.size()));
      //contact->callID.append(alphabet[1]);
    }

    qDebug() << "Generated CallID: " << contact->callID;

    contact->port = 5060; // use 5061 for tls encrypted

    contact->cseq = 1;

    for( unsigned int i = 0; i < callIDLength/2; ++i )
    {
      contact->ourTag.append(alphabet.at(qrand()%alphabet.size()));
    }
    qDebug() << "Generated tag: " << contact->ourTag;
    contact->sdp = sdp;

    negotiations_[contact->callID] = std::move(contact);
  }
}
