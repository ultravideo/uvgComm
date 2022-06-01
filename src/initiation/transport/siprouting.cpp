#include "siprouting.h"

#include "settingskeys.h"

#include "common.h"
#include "logger.h"
#include "sipfieldparsinghelper.h" // we need to parse GRUU URI

#include <QSettings>

SIPRouting::SIPRouting(std::shared_ptr<TCPConnection> connection):
  connection_(connection),
  received_(""),
  rport_(0),
  resetRegistrations_(false)
{}


void SIPRouting::processOutgoingRequest(SIPRequest& request, QVariant& content)
{
  Q_UNUSED(content)

  Logger::getLogger()->printNormal(this, "Processing outgoing request");

  if (connection_ == nullptr)
  {
    Logger::getLogger()->printProgramError(this, "No connection set!");
    return;
  }

  if (!connection_->waitUntilConnected())
  {
    Logger::getLogger()->printWarning(this, "Socket is not connected when sending request");
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

  if (request.method == SIP_INVITE || request.method == SIP_REGISTER)
  {
    addContactField(request.message,
                    connection_->localAddress(),
                    connection_->localPort(),
                    DEFAULT_SIP_TYPE,
                    request.method);

    if (request.method == SIP_REGISTER)
    {
      addREGISTERContactParameters(request.message);
      addToSupported("path", request.message); // RFC 5626 section 4.2.1
    }

    if (resetRegistrations_ && request.message->expires != nullptr)
    {
      // when we detect an rport difference, we remove our existing registration first
      *request.message->expires = 0;
    }
  }

  if (request.method == SIP_INVITE ||
      request.method == SIP_OPTIONS ||
      request.method == SIP_REGISTER)
  {
    addToSupported("gruu", request.message);
  }

  if (request.method != SIP_ACK)
  {
    addToSupported("outbound", request.message); // RFC 5626 section 4.2.1
  }


  emit outgoingRequest(request, content);

}


void SIPRouting::processOutgoingResponse(SIPResponse& response, QVariant& content)
{
  Q_UNUSED(content)

  Logger::getLogger()->printNormal(this, "Processing outgoing response");

  if (!connection_->waitUntilConnected())
  {
    Logger::getLogger()->printWarning(this, "Socket is not connected when sending a response");
    return;
  }

  if (response.message->cSeq.method == SIP_INVITE && response.type == SIP_OK)
  {
    addToSupported("gruu", response.message);

    addContactField(response.message,
                      connection_->localAddress(),
                      connection_->localPort(),
                      DEFAULT_SIP_TYPE,
                      response.message->cSeq.method);
  }

  emit outgoingResponse(response, content);
}


void SIPRouting::processIncomingResponse(SIPResponse& response, QVariant& content,
                                         bool retryRequest)
{
  Q_UNUSED(content)

  /*
  if (connection_ && connection_->isConnected())
  {

     *TODO: This check does not work in all situations. Sometimes a router decides
     * to change via for INVITE, but not REGISTER which would meaning we have no
     * way of knowing what is our outside NAT address when we receive message to it.
     *
    if (!isMeantForUs(response.message->vias,
                       connection_->localAddress(),
                       connection_->localPort()))
    {
      Logger::getLogger()->printError(this, "Received a response where our "
                                            "via is not the only one!");
      return;
    }

  }
  else
  {
    Logger::getLogger()->printError(this, "Not connected when checking response via field");
    return;
  }
  */

  if (response.message->cSeq.method == SIP_REGISTER)
  {
    resetRegistrations_ = false; // resetting here avoids loops with registrar

    if (response.type == SIP_OK)
    {
      // we always use gruus if they are available and don't bother with other methods
      if (!getGruus(response.message))
      {
        // rport is another method around NAT, but it is not supported by all routers for
        // TCP because it is technically an UDP feature
        if (isRportDifferent(response.message->vias,
                             connection_->localAddress(),
                             connection_->localPort()))
        {
          retryRequest = true;

          if (response.message->contact.empty())
          {
            updateReceiveAddressAndRport(response.message->vias,
                                         connection_->localAddress(),
                                         connection_->localPort());
          }
          else
          {
            // we are registered so we have to remove the registration
            resetRegistrations_ = true;
          }
        }
      }
    }
    else
    {
      updateReceiveAddressAndRport(response.message->vias,
                                   connection_->localAddress(),
                                   connection_->localPort());
    }
  }

  emit incomingResponse(response, content, retryRequest);
}


void SIPRouting::updateReceiveAddressAndRport(QList<ViaField>& vias,
                                          QString localAddress,
                                          uint16_t localPort)
{
  // find the via with our address and port
  for (ViaField& via : vias)
  {
    if (via.sentBy == localAddress && via.port == localPort)
    {
      received_ = via.receivedAddress;
      rport_ = via.rportValue;
      return;
    }
  }
}


bool SIPRouting::isRportDifferent(QList<ViaField> &vias,
                                  QString localAddress,
                                  uint16_t localPort)
{
  for (ViaField& via : vias)
  {
    if (via.sentBy == localAddress && via.port == localPort)
    {
      if ((via.rportValue != localPort || via.receivedAddress != localAddress) &&
          (via.rportValue != rport_ || via.receivedAddress != received_))
      {
        Logger::getLogger()->printNormal(this, "Our receive address is different, meaning we are behind NAT",
                                         {"Address"}, {via.receivedAddress + ":" + QString::number(via.rportValue)});
        return true;
      }
    }
  }
  return false;
}


bool SIPRouting::isMeantForUs(QList<ViaField> &vias,
                  QString localAddress,
                  uint16_t localPort)
{
  // find the via with our address and port
  if (vias.size() == 1)
  {
    if (vias.at(0).sentBy == localAddress && vias.at(0).port == localPort)
    {
      Logger::getLogger()->printNormal(this, "Found our via. This is meant for us!");
      return true;
    }

    if (vias.at(0).sentBy == received_ && vias.at(0).port == rport_)
    {
      Logger::getLogger()->printNormal(this, "Via matches our rport. Must be for us!");
      return true;
    }
  }

  return false;
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
    message->vias.back().alias = true;
  }

  previousVia_ = message->vias.back();
}


void SIPRouting::addContactField(std::shared_ptr<SIPMessageHeader> message,
                                 QString localAddress, uint16_t localPort,
                                 SIPType type, SIPRequestMethod method)
{
  message->contact.push_back({{"", SIP_URI{type, {getLocalUsername(), ""}, {"", 0}, {}, {}}}, {}});

  // There are four different alternatives: use public GRUU if we have it,
  // otherwise use temporary GRUU or rport if we have those

  if (tempGruu_ != "" && method != SIP_REGISTER)
  {
    if (!parseURI(tempGruu_, message->contact.back().address.uri))
    {
      Logger::getLogger()->printProgramError(this, "Failed to parse Temporary GRUU for contact field");
    }
  }
  else if (pubGruu_ != "" && method != SIP_REGISTER)
  {
    if (!parseURI(pubGruu_, message->contact.back().address.uri))
    {
      Logger::getLogger()->printProgramError(this, "Failed to parse Public GRUU for contact field");
    }
  }
  else if (received_ != "" && rport_ != 0)
  {
    // use received address and rport if we have them, works behind nat, but not all
    // routers seem to support these parameters. GRUU works always
    message->contact.back().address.uri.hostport.host = received_;
    message->contact.back().address.uri.hostport.port = rport_;
  }
  else // otherwise use localaddress, works when not behind NAT
  {
    message->contact.back().address.uri.hostport.host = localAddress;
    message->contact.back().address.uri.hostport.port = localPort;
  }
}


void SIPRouting::addREGISTERContactParameters(std::shared_ptr<SIPMessageHeader> message)
{
  if (message->contact.empty())
  {
    Logger::getLogger()->printProgramWarning(this,
                                             "Please add contact field before adding parameters");
  }

  QSettings settings("kvazzup.ini", QSettings::IniFormat);
  message->contact.back().parameters.push_back({"reg-id", "1"});
  message->contact.back().parameters.push_back({"+sip.instance", "\"<urn:uuid:" +
                                                settings.value(SettingsKey::sipUUID).toString() + ">\""});
}


void SIPRouting::addToSupported(QString feature, std::shared_ptr<SIPMessageHeader> message)
{
  if (message->supported == nullptr)
  {
    message->supported = std::shared_ptr<QStringList>(new QStringList);
  }

  message->supported->append(feature);
}


bool SIPRouting::getGruus(std::shared_ptr<SIPMessageHeader> message)
{
  QString tempGruu = "";
  QString pubGruu = "";

  for (auto& contact : message->contact)
  {
    for (int i = 0; i < contact.parameters.size() && tempGruu == ""; ++i)
    {
      // TODO: Check that pub-gruu does not change from what we sent
      if (contact.parameters.at(i).name == "pub-gruu")
      {
        pubGruu = contact.parameters.at(i).value;
        Logger::getLogger()->printNormal(this, "Found public GRUU", "pub-gruu", pubGruu);
      }
    }

    for (int i = 0; i < contact.parameters.size() && tempGruu == ""; ++i)
    {
      if (contact.parameters.at(i).name == "temp-gruu")
      {
        tempGruu = contact.parameters.at(i).value;
        Logger::getLogger()->printNormal(this, "Found temporary GRUU", "temp-gruu", tempGruu);
      }
    }
  }

  tempGruu_ = tempGruu;
  pubGruu_ = pubGruu;

  return tempGruu != "" || pubGruu != "";
}
