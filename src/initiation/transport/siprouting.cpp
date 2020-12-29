#include "siprouting.h"

#include <QSettings>

#include "common.h"


SIPRouting::SIPRouting():
  contactAddress_(""),
  contactPort_(0),
  first_(true)
{}

void SIPRouting::processResponseViaFields(QList<ViaField>& vias,
                                          QString localAddress,
                                          uint16_t localPort)
{
  // find the via with our address and port

  for (ViaField& via : vias)
  {
    if (via.sentBy == localAddress && via.port == localPort)
    {
      printDebug(DEBUG_NORMAL, "SIPRouting", "Found our via. This is meant for us!");

      if (via.rportValue != 0 && via.receivedAddress != "")
      {
        printDebug(DEBUG_NORMAL, "SIPRouting", "Found our received address and rport",
                  {"Address"}, {via.receivedAddress + ":" + QString::number(via.rportValue)});

        // we do not update our address, because we want to remove this registration
        // first before updating.
        if (first_)
        {
          first_ = false;
        }
        else
        {
          contactAddress_ = via.receivedAddress;
          contactPort_ = via.rportValue;
        }
      }
      return;
    }
  }
}


void SIPRouting::getViaAndContact(std::shared_ptr<SIPMessageHeader> message,
                                   QString localAddress,
                                   uint16_t localPort)
{
  // set via-address
  if (!message->vias.empty())
  {
    message->vias.back().sentBy = localAddress;
    message->vias.back().port = localPort;
  }

  getContactAddress(message, localAddress, localPort, DEFAULT_SIP_TYPE);
}


void SIPRouting::getContactAddress(std::shared_ptr<SIPMessageHeader> message,
                                   QString localAddress, uint16_t localPort,
                                   SIPType type)
{
  message->contact = {{"", {type, {getLocalUsername(), ""}, {"", 0}, {}, {}}}, {}};

    // use rport address and port if we have them, otherwise use localaddress
  if (contactAddress_ != "")
  {
    message->contact.address.uri.hostport.host = contactAddress_;
  }
  else
  {
    message->contact.address.uri.hostport.host = localAddress;
  }

  if (contactPort_ != 0)
  {
    message->contact.address.uri.hostport.port = contactPort_;
  }
  else
  {
    message->contact.address.uri.hostport.port = localPort;
  }
}
