#include "siprouting.h"

#include <QDebug>


SIPRouting::SIPRouting():
  viaAddress_(""),
  viaPort_(0),
  contactAddress(""),
  contactPort_(0)
{}


void SIPRouting::connectionEstablished(QString localAddress, uint16_t localPort)
{
  viaAddress_ = localAddress;
  viaPort_ = localPort;

  contactAddress = localAddress;
  contactPort_ = localPort;
}


void SIPRouting::processResponseViaFields(QList<ViaInfo>& vias)
{
  // find the via with our address and port

  for (ViaInfo& via : vias)
  {
    if (via.address == viaAddress_ && via.port == viaPort_)
    {
      qDebug() << "Found our via!";

      if (via.rportValue != 0 && via.receivedAddress != "")
      {
        qDebug() << "Found rport:" << via.receivedAddress << ":" << via.rportValue;
        contactAddress = via.receivedAddress;
        contactPort_ = via.rportValue;
      }
      return;
    }
  }
}


void SIPRouting::getRequestRouting(std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(viaPort_ != 0);

  // set via-address
  if (!message->vias.empty())
  {
    message->vias.back().address = viaAddress_;
    message->vias.back().port = viaPort_;
  }

  // set contact address
  message->contact.host = contactAddress;
  message->contact.port = contactPort_;
}


void SIPRouting::getResponseContact(std::shared_ptr<SIPMessageInfo> message)
{
  // set contact address
  message->contact.host = contactAddress;
  message->contact.port = contactPort_;
}
