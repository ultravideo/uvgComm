#include "siprouting.h"

#include "settingskeys.h"

#include "common.h"
#include "logger.h"
#include "sipfieldparsinghelper.h" // we need to parse GRUU URI

#include <QSettings>

SIPRouting::SIPRouting(std::shared_ptr<TCPConnection> connection):
  connection_(connection),
  contactAddress_(""),
  contactPort_(0),
  first_(true)
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

  // TODO: Handle this better
  if (!connection_->isConnected())
  {
    Logger::getLogger()->printWarning(this, "Socket not connected");
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
                    DEFAULT_SIP_TYPE);

    if (request.method == SIP_REGISTER)
    {
      addREGISTERContactParameters(request.message);
    }
  }

  if (request.method == SIP_INVITE ||
      request.method == SIP_OPTIONS ||
      request.method == SIP_REGISTER)
  {
    addGruuToSupported(request.message);
  }

  emit outgoingRequest(request, content);

}


void SIPRouting::processOutgoingResponse(SIPResponse& response, QVariant& content)
{
  Q_UNUSED(content)

  Logger::getLogger()->printNormal(this, "Processing outgoing response");

  // TODO: Handle this better
  if (!connection_->isConnected())
  {
    Logger::getLogger()->printWarning(this, "Socket not connected");
    return;
  }

  if (response.message->cSeq.method == SIP_INVITE && response.type == SIP_OK)
  {
    addGruuToSupported(response.message);

    addContactField(response.message,
                      connection_->localAddress(),
                      connection_->localPort(),
                      DEFAULT_SIP_TYPE);
  }

  emit outgoingResponse(response, content);
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
    Logger::getLogger()->printError(this, "Not connected when checking response via field");
  }

  if (response.type == SIP_OK && response.message->cSeq.method == SIP_REGISTER)
  {
    getGruus(response.message);
  }

  emit incomingResponse(response, content);
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
      Logger::getLogger()->printNormal(this, "Found our via. This is meant for us!");

      if (via.rportValue != 0 && via.receivedAddress != "")
      {
        Logger::getLogger()->printNormal(this, "Found our received address and rport",
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
    message->vias.back().alias = true;
  }

  previousVia_ = message->vias.back();
}


void SIPRouting::addContactField(std::shared_ptr<SIPMessageHeader> message,
                                 QString localAddress, uint16_t localPort,
                                 SIPType type)
{
  message->contact.push_back({{"", SIP_URI{type, {getLocalUsername(), ""}, {"", 0}, {}, {}}}, {}});

  // There are four different alternatives: use public GRUU if we have it,
  // otherwise use temporary GRUU or rport if we have those
  if (pubGruu_ != "")
  {
    if (!parseURI(pubGruu_, message->contact.back().address.uri))
    {
      Logger::getLogger()->printProgramError(this, "Failed to parse Public GRUU for contact field");
    }
  }
  else if (tempGruu_ != "")
  {
    if (!parseURI(tempGruu_, message->contact.back().address.uri))
    {
      Logger::getLogger()->printProgramError(this, "Failed to parse Temporary GRUU for contact field");
    }
  }
  else if (contactAddress_ != "" && contactPort_ != 0)
  {
    // use rport address and port if we have them
    message->contact.back().address.uri.hostport.host = contactAddress_;
    message->contact.back().address.uri.hostport.port = contactPort_;
  }
  else // otherwise use localaddress
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


void SIPRouting::addGruuToSupported(std::shared_ptr<SIPMessageHeader> message)
{
  if (message->supported == nullptr)
  {
    message->supported = std::shared_ptr<QStringList>(new QStringList);
  }

  message->supported->append("gruu");
}

void SIPRouting::getGruus(std::shared_ptr<SIPMessageHeader> message)
{
  QString tempGruu = "";
  QString pubGruu = "";

  for (auto& contact : message->contact)
  {
    for (int i = 0; i < contact.parameters.size() && tempGruu == ""; ++i)
    {
      // TODO: Check that pub-gruu does not change
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
}
