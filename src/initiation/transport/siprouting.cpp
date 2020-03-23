#include "siprouting.h"

#include <QSettings>

#include "common.h"


SIPRouting::SIPRouting():
  contactAddress_(""),
  contactPort_(0)
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

        contactAddress_ = via.receivedAddress;
        contactPort_ = via.rportValue;
      }
      return;
    }
  }
}


void SIPRouting::getViaAndContact(std::shared_ptr<SIPMessageInfo> message,
                                   QString localAddress,
                                   uint16_t localPort)
{
  // set via-address
  if (!message->vias.empty())
  {
    message->vias.back().address = localAddress;
    message->vias.back().port = localPort;
  }

  getContactAddress(message, localAddress, localPort, TCP);
}


void SIPRouting::getContactAddress(std::shared_ptr<SIPMessageInfo> message,
                                   QString localAddress, uint16_t localPort, ConnectionType type)
{
  message->contact = {type, getUsername(), "", "", 0, {}};

    // use rport address and port if we have them, otherwise use localaddress
  if (contactAddress_ != "")
  {
    message->contact.host = contactAddress_;
  }
  else
  {
    message->contact.host = localAddress;
  }

  if (contactPort_ != 0)
  {
    message->contact.port = contactPort_;
  }
  else
  {
    message->contact.port = localPort;
  }
}


QString SIPRouting::getUsername()
{
  QSettings settings("kvazzup.ini", QSettings::IniFormat);

  return settings.value("local/Username").toString();
}
