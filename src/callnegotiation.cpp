#include "callnegotiation.h"


//TODO use cryptographically secure callID generation!!

const QString alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                         "abcdefghijklmnopqrstuvwxyz"
                         "0123456789";

const uint16_t callIDLength = 32;

CallNegotiation::CallNegotiation()
{}

CallNegotiation::~CallNegotiation()
{

}

void CallNegotiation::init()
{

  qsrand(1);
}

void CallNegotiation::startCall(QList<Contact> addresses, QString sdp)
{
  qDebug() << "";
  for (int i = 0; i < addresses.size(); ++i)
  {
    std::unique_ptr<SIPLink> contact (new SIPLink);

    contact->peer.address = addresses.at(i).address;

    for( unsigned int i = 0; i < callIDLength; ++i )
    {
      contact->callID.append(qrand()%alphabet.size());
    }

    qDebug() << "Generated CallID: " << contact->callID;

    contact->port = 5060; // use 5061 for tls encrypted

    contact->cseq = 1;

    for( unsigned int i = 0; i < callIDLength; ++i )
    {
      contact->ourTag.append(qrand()%alphabet.size());
    }
    contact->sdp = sdp;

    negotiations_[contact->callID] = ;
  }
}
