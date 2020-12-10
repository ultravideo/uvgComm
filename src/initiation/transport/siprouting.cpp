#include "siprouting.h"

#include <QSettings>

#include "common.h"


SIPRouting::SIPRouting():
  contactAddress_(""),
  contactPort_(0),
  first_(true)
{}

void SIPRouting::processResponseViaFields(QList<ViaInfo>& vias,
                                          QString localAddress,
                                          uint16_t localPort)
{
  // find the via with our address and port

  for (ViaInfo& via : vias)
  {
    if (via.address == localAddress && via.port == localPort)
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


void SIPRouting::getViaAndContact(std::shared_ptr<SIPMessageBody> message,
                                   QString localAddress,
                                   uint16_t localPort)
{
  // set via-address
  if (!message->vias.empty())
  {
    message->vias.back().address = localAddress;
    message->vias.back().port = localPort;
  }

  getContactAddress(message, localAddress, localPort, DEFAULTSIPTYPE);
}


void SIPRouting::getContactAddress(std::shared_ptr<SIPMessageBody> message,
                                   QString localAddress, uint16_t localPort,
                                   SIPType type)
{
  message->contact = {type, {getLocalUsername(), ""}, "", {"", 0}, {}};

    // use rport address and port if we have them, otherwise use localaddress
  if (contactAddress_ != "")
  {
    message->contact.hostport.host = contactAddress_;
  }
  else
  {
    message->contact.hostport.host = localAddress;
  }

  if (contactPort_ != 0)
  {
    message->contact.hostport.port = contactPort_;
  }
  else
  {
    message->contact.hostport.port = localPort;
  }
}
