#include "siprouting.h"

#include <QSettings>

#include "common.h"


SIPRouting::SIPRouting(std::shared_ptr<TCPConnection> connection):
  connection_(connection),
  contactAddress_(""),
  contactPort_(0),
  first_(true)
{}


void SIPRouting::processOutgoingRequest(SIPRequest& request, QVariant& content)
{
  Q_UNUSED(content)

  if (connection_ == nullptr)
  {
    printProgramError(this, "No connection set!");
    return;
  }

  // TODO: Handle this better
  if (!connection_->isConnected())
  {
    printWarning(this, "Socket not connected");
    return;
  }

  if (request.method != SIP_CANCEL)
  {
    addVia(request.method, request.message,
           connection_->localAddress(),
           connection_->localPort());
  }
  else
  {
    request.message->vias.push_front(previousVia_);
  }

  if (request.method == SIP_INVITE)
  {
    addContactField(request.message,
                    connection_->localAddress(),
                    connection_->localPort(),
                    DEFAULT_SIP_TYPE);
  }

}


void SIPRouting::processOutgoingResponse(SIPResponse& response, QVariant& content)
{
  Q_UNUSED(content)

  // TODO: Handle this better
  if (!connection_->isConnected())
  {
    printWarning(this, "Socket not connected");
    return;
  }

  if (response.message->cSeq.method == SIP_INVITE && response.type == SIP_OK)
  {
    addContactField(response.message,
                      connection_->localAddress(),
                      connection_->localPort(),
                      DEFAULT_SIP_TYPE);
  }
}


void SIPRouting::processIncomingResponse(SIPResponse& response, QVariant& content)
{
  Q_UNUSED(content)

  if (connection_ && connection_->isConnected())
  {
    processResponseViaFields(response.message->vias,
                             connection_->localAddress(),
                             connection_->localPort());
  }
  else
  {
    printError(this, "Not connected when checking response via field");
  }
}


void SIPRouting::processResponseViaFields(QList<ViaField>& vias,
                                          QString localAddress,
                                          uint16_t localPort)
{
  // find the via with our address and port

  for (ViaField& via : vias)
  {
    if (via.sentBy == localAddress && via.port == localPort)
    {
      printNormal(this, "Found our via. This is meant for us!");

      if (via.rportValue != 0 && via.receivedAddress != "")
      {
        printNormal(this, "Found our received address and rport",
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


void SIPRouting::addVia(SIPRequestMethod type,
                        std::shared_ptr<SIPMessageHeader> message,
                        QString localAddress,
                        uint16_t localPort)
{
  ViaField via = ViaField{SIP_VERSION, DEFAULT_TRANSPORT, localAddress, localPort,
      QString(MAGIC_COOKIE + generateRandomString(BRANCH_TAIL_LENGTH)),
      false, false, 0, "", {}};

  message->vias.push_back(via);

  if (type == SIP_INVITE || type == SIP_ACK)
  {
    message->vias.back().rport = true;
  }

  if (type == SIP_REGISTER)
  {
    message->vias.back().rport = true;
  }

  previousVia_ = message->vias.back();
}


void SIPRouting::addContactField(std::shared_ptr<SIPMessageHeader> message,
                                 QString localAddress, uint16_t localPort,
                                 SIPType type)
{
  message->contact.push_back({{"", SIP_URI{type, {getLocalUsername(), ""}, {"", 0}, {}, {}}}, {}});

    // use rport address and port if we have them, otherwise use localaddress
  if (contactAddress_ != "")
  {
    message->contact.back().address.uri.hostport.host = contactAddress_;
  }
  else
  {
    message->contact.back().address.uri.hostport.host = localAddress;
  }

  if (contactPort_ != 0)
  {
    message->contact.back().address.uri.hostport.port = contactPort_;
  }
  else
  {
    message->contact.back().address.uri.hostport.port = localPort;
  }
}
