#include "networkcandidates.h"

#include "common.h"
#include "logger.h"
#include "settingskeys.h"

#include <QNetworkInterface>
#include <QUdpSocket>
#include <QDebug>

const uint16_t STUNADDRESSPOOL = 8;

const int AN_HOUR = 1000 * 60 * 60;
const int STUN_INTERVAL_PERIOD = 100;

const int NUMBER_OF_POSSIBLE_PORTS = 1000;


NetworkCandidates::NetworkCandidates():
  requests_(),
  stunMutex_(),
  stunAddresses_(),
  stunBindings_(),
  portLock_(),
  availablePorts_(),
  reservedPorts_(),
  behindNAT_(true), // assume that we are behind NAT at first
  currentMinPort_(0),
  stunServerAddress_(""),
  stunPort_(0)
{}


NetworkCandidates::~NetworkCandidates()
{
  requests_.clear();
}


void NetworkCandidates::init()
{
  stunMutex_.lock();

  // clear previous STUN stuff if it is not needed
  if (!settingEnabled(SettingsKey::sipSTUNEnabled) ||
      settingString(SettingsKey::sipSTUNAddress) != stunServerAddress_)
  {
    requests_.clear();
    stunAddresses_.clear();
    stunBindings_.clear();
    stunServerIP_.clear();

    if (reservedPorts_.find(0) != reservedPorts_.end())
    {
      reservedPorts_[0].clear();
    }
  }
  stunMutex_.unlock();


  int minPort = settingValue(SettingsKey::sipMediaPort);
  int maxPort = minPort + NUMBER_OF_POSSIBLE_PORTS;

  // if the settings have changed
  if (currentMinPort_ != minPort)
  {
    // take all currently reserved ports so we don't make them available again
    QList<std::pair<QString, uint16_t>> all_reserved;
    for (auto& reserved : reservedPorts_)
    {
      all_reserved += reserved.second;
    }

    availablePorts_.clear();
    currentMinPort_ = minPort;

    Logger::getLogger()->printDebug(DEBUG_NORMAL, this, 
                                    "Allocating (but not reserving) ports for media",
                                    {"Min ports", "Max port"},
                                    {QString::number(minPort), QString::number(maxPort)});

    foreach (const QHostAddress& address, QNetworkInterface::allAddresses())
    {
      if (address.protocol() == QAbstractSocket::IPv4Protocol &&
          !address.isLoopback())
      {
        if (sanityCheck(address, minPort))
        {
          portLock_.lock();
          availablePorts_.insert(std::pair<QString, std::deque<uint16_t>>(address.toString(),{}));
          portLock_.unlock();

          for(uint16_t i = minPort; i < maxPort; ++i)
          {
            // only make available if it was not previously reserved
            if (!all_reserved.contains(std::pair<QString, uint16_t>(address.toString(), i)))
            {
              makePortAvailable(address.toString(), i);
            }
          }
        }
      }
    }
  }

  // Start stun address acquasition if stun was enabled or address has changed
  if (settingEnabled(SettingsKey::sipSTUNEnabled) &&
      (!stunEnabled_ ||
      stunServerAddress_ != settingString(SettingsKey::sipSTUNAddress) ||
      stunPort_ != settingValue(SettingsKey::sipSTUNPort)))
  {
    stunServerAddress_ = settingString(SettingsKey::sipSTUNAddress);
    stunPort_ = settingValue(SettingsKey::sipSTUNPort);

    Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Looking up STUN server IP",
                                    {"Server address"}, {stunServerAddress_});

    if (stunServerAddress_ != "")
    {
      stunMutex_.lock();
      // get ip address of stun server
      wantAddress(stunServerAddress_);

      QObject::connect(&refreshSTUNTimer_,  &QTimer::timeout,
                       this,                &NetworkCandidates::refreshSTUN);

      refreshSTUNTimer_.setInterval(STUN_INTERVAL_PERIOD);
      refreshSTUNTimer_.setSingleShot(false);
      refreshSTUNTimer_.start();
      stunMutex_.unlock();
    }
    else
    {
      Logger::getLogger()->printWarning(this, "Invalid STUN server address found in settings");
    }
  }

  // record current stun state so we can check it next time
  stunEnabled_ = settingEnabled(SettingsKey::sipSTUNEnabled);
}


void NetworkCandidates::refreshSTUN()
{
  if (!settingEnabled(SettingsKey::sipSTUNEnabled))
  {
    return;
  }

  stunMutex_.lock();
  if (!stunServerIP_.isNull() &&
      stunAddresses_.size() + requests_.size() < STUNADDRESSPOOL)
  {
    // Send STUN-Request through all the interfaces, since we don't know which is
    // connected to the internet. Most of them will fail.
    moreSTUNCandidates();
  }
  stunMutex_.unlock();

  // The socket unbinding cannot happen in processreply, because that would destroy the socket
  // leading to heap corruption. Instead we do the unbinding here.

  QStringList removed;
  for (auto& request : requests_)
  {
    if (request.second->finished)
    {
      request.second->udp.unbind();
      removed.push_back(request.first);
    }
  }

  // remove requests so we know how many reaquests we have going on.
  for (auto& removal : removed)
  {
    requests_.erase(removal);
    //Logger::getLogger()->printNormal(this, "Removed", {"Left"}, 
    //                                 {QString::number(requests_.size())});
  }
}


void NetworkCandidates::createSTUNCandidate(QHostAddress local, quint16 localPort,
                                            QHostAddress stun, quint16 stunPort)
{
  if (stun == QHostAddress(""))
  {
    Logger::getLogger()->printWarning(this, "Failed to resolve our public IP! "
                                      "Server-reflexive candidates won't be created!");
    return;
  }

  // are we actually behind NAT
  if (local == stun)
  {
    Logger::getLogger()->printNormal(this, "We don't seem to be behind NAT");
    behindNAT_ = false;
    refreshSTUNTimer_.setInterval(AN_HOUR);

    stunMutex_.lock();
    stunAddresses_.clear();
    stunBindings_.clear();
    stunMutex_.unlock();

    makePortAvailable(local.toString(), localPort);
  }
  else
  {
    behindNAT_ = true;
    refreshSTUNTimer_.setInterval(STUN_INTERVAL_PERIOD);
    Logger::getLogger()->printNormal(this, "Created ICE STUN candidate", 
                                     {"STUN Translation"},
                                     {local.toString() + ":" + QString::number(localPort) + 
                                      " << " +
                                      stun.toString() + ":" + QString::number(stunPort)});

    stunMutex_.lock();
    stunAddresses_.push_back({stun, stunPort});
    stunBindings_.push_back({local, localPort});
    stunMutex_.unlock();
  }
}


std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> NetworkCandidates::localCandidates(
    uint8_t streams, uint32_t sessionID)
{
  std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> addresses
      =   std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> (
        new QList<std::pair<QHostAddress, uint16_t>>());

  for (auto& interface : availablePorts_)
  {
    if (isPrivateNetwork(interface.first) && availablePorts_[interface.first].size() >= streams)
    {
      for (unsigned int i = 0; i < streams; ++i)
      {
        addresses->push_back({QHostAddress(interface.first),
                              nextAvailablePort(interface.first, sessionID)});
      }
    }
  }

  return addresses;
}


std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> NetworkCandidates::globalCandidates(
    uint8_t streams, uint32_t sessionID)
{
  std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> addresses
      =   std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> (
        new QList<std::pair<QHostAddress, uint16_t>>());

  for (auto& interface : availablePorts_)
  {
    if (!isPrivateNetwork(interface.first) &&
        availablePorts_[interface.first].size() >= streams)
    {
      for (unsigned int i = 0; i < streams; ++i)
      {
        addresses->push_back({QHostAddress(interface.first),
                              nextAvailablePort(interface.first, sessionID)});
      }
    }
  }
  return addresses;
}


std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> NetworkCandidates::stunCandidates(
    uint8_t streams)
{ 
  std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> addresses
      =   std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> (
        new QList<std::pair<QHostAddress, uint16_t>>());

  if (!settingEnabled(SettingsKey::sipSTUNEnabled))
  {
    return addresses;
  }

  if (streams <= STUNADDRESSPOOL)
  {
    stunMutex_.lock();

    // TODO:  We should use separate thread to get STUN candidates so we can
    // wait for them if we don't have enough. This is more relevant in conference calls,
    // where more candidates might be needed fast.

    if (stunAddresses_.size() < streams)
    {
      // the STUN candidates havent had time to refresh yet

      stunMutex_.unlock();


      // qSleep(STUN_INTERVAL_PERIOD*(stunAddresses_.size() - streams));
      stunMutex_.lock();

    }

    if (stunAddresses_.size() >= streams)
    {
      for (unsigned int i = 0; i < streams; ++i)
      {
        std::pair<QHostAddress, uint16_t> address = stunAddresses_.front();
        stunAddresses_.pop_front();
        addresses->push_back(address);
      }

    }
    else
    {
      Logger::getLogger()->printWarning(this, "Not enough STUN candidates found!");
    }

    stunMutex_.unlock();
  }
  else
  {
    Logger::getLogger()->printWarning(this, "The STUN address pool is too small!");
  }


  return addresses;
}


std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> NetworkCandidates::stunBindings(
    uint8_t streams, uint32_t sessionID)
{
  std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> addresses
      =   std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> (
        new QList<std::pair<QHostAddress, uint16_t>>());

  if (!settingEnabled(SettingsKey::sipSTUNEnabled))
  {
    return addresses;
  }

  if (streams <= STUNADDRESSPOOL)
  {
    stunMutex_.lock();
    if (stunBindings_.size() >= streams)
    {
      for (unsigned int i = 0; i < streams; ++i)
      {
        std::pair<QHostAddress, uint16_t> address = stunBindings_.front();
        stunBindings_.pop_front();

        addresses->push_back(address);

        reservedPorts_[sessionID].push_back(
              std::pair<QString, uint16_t>({address.first.toString(), address.second}));
      }
    }
    else
    {
      Logger::getLogger()->printWarning(this, "No STUN bindings added!");
    }
    stunMutex_.unlock();
  }

  return addresses;
}


std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> NetworkCandidates::turnCandidates(
    uint8_t streams, uint32_t sessionID)
{
  Q_UNUSED(streams);
  Q_UNUSED(sessionID);


  // TODO: We should add possiblity to use TURN server. Add those candidates to this list.

  std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> addresses
      =   std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> (
        new QList<std::pair<QHostAddress, uint16_t>>());

  return addresses;
}


uint16_t NetworkCandidates::nextAvailablePort(QString interface, uint32_t sessionID)
{
  uint16_t nextPort = 0;

  portLock_.lock();

  if (availablePorts_.find(interface) == availablePorts_.end() ||
      availablePorts_[interface].size() == 0)
  {
    portLock_.unlock();
    Logger::getLogger()->printWarning(this, "Either couldn't find interface or "
                       "this interface has run out of ports");
    return 0;
  }

  nextPort = availablePorts_[interface].at(0);
  availablePorts_[interface].pop_front();
  reservedPorts_[sessionID].push_back(std::pair<QString, uint16_t>(interface, nextPort));

  // TODO: Check that port works.
  portLock_.unlock();

  return nextPort;
}

void NetworkCandidates::makePortAvailable(QString interface, uint16_t port)
{
  if(port != 0)
  {
    portLock_.lock();
    if (availablePorts_.find(interface) == availablePorts_.end())
    {
      portLock_.unlock();
      Logger::getLogger()->printWarning(this, "Couldn't find interface "
                                              "when making port available.");
      return;
    }

    availablePorts_[interface].push_back(port);
    portLock_.unlock();
  }
}


/* https://en.wikipedia.org/wiki/Private_network#Private_IPv4_addresses */
bool NetworkCandidates::isPrivateNetwork(const QString& address)
{
  if (address.startsWith("10.") || address.startsWith("192.168"))
  {
    return true;
  }

  if (address.startsWith("172."))
  {
    int octet = address.split(".")[1].toInt();

    if (octet >= 16 && octet <= 31)
    {
      return true;
    }
  }

  return false;
}


void NetworkCandidates::cleanupSession(uint32_t sessionID)
{
  if (reservedPorts_.find(sessionID) == reservedPorts_.end())
  {
    Logger::getLogger()->printWarning(this, "Tried to cleanup session "
                                            "with no reserved ports");
    return;
  }

  for(auto& session : reservedPorts_[sessionID])
  {
    makePortAvailable(session.first, session.second);
  }
  reservedPorts_.erase(sessionID);
}


void NetworkCandidates::wantAddress(QString stunServer)
{
  // To find the IP address of qt-project.org
  QHostInfo::lookupHost(stunServer, this, SLOT(handleStunHostLookup(QHostInfo)));
}


void NetworkCandidates::handleStunHostLookup(QHostInfo info)
{
  if (availablePorts_.empty())
  {
    Logger::getLogger()->printProgramError(this, "Interfaces have not been set "
                                                 "before sending STUN requests");
    return;
  }

  const auto addresses = info.addresses();
  if(addresses.size() != 0)
  {
    stunServerIP_ = addresses.at(0);
  }
  else
  {
    return;
  }
}

void NetworkCandidates::moreSTUNCandidates()
{
  if (!stunServerIP_.isNull())
  {
    for (auto& interface : availablePorts_)
    {
      int stunPort = settingValue(SettingsKey::sipSTUNPort);

      if (stunPort != 0)
      {
        // use 0 as STUN sessionID
        sendSTUNserverRequest(QHostAddress(interface.first), nextAvailablePort(interface.first, 0),
                              stunServerIP_,                 stunPort);
      }
      else
      {
        Logger::getLogger()->printWarning(this, "An invalid zero found as "
                                                "STUN server port settings value");
      }
    }
  }
  else
  {
    Logger::getLogger()->printProgramError(this, "STUN server address not set!");
  }
}


void NetworkCandidates::sendSTUNserverRequest(QHostAddress localAddress,
                                              uint16_t localPort,
                                              QHostAddress serverAddress,
                                              uint16_t serverPort)
{
  if (localPort == 0 || serverPort == 0)
  {
    Logger::getLogger()->printProgramError(this, "Not port set. Can't get STUN address.");
    return;
  }

  //Logger::getLogger()->printNormal(this, "Sending STUN server request", {"Path"},
  //        {localAddress.toString() + ":" + QString::number(localPort) + " -> " +
  //         serverAddress.toString() + ":" + QString::number(serverPort)});

  QString key = localAddress.toString() + ":" + QString::number(localPort);

  if (requests_.find(key) == requests_.end())
  {
    requests_[key] = std::shared_ptr<STUNRequest>(new STUNRequest);
    requests_[key]->finished = false;
  }

  QObject::connect(&requests_[key]->udp, &UDPServer::datagramAvailable,
                   this,              &NetworkCandidates::processSTUNReply);

  if (!requests_[key]->udp.bindSocket(localAddress, localPort))
  {
    requests_.erase(key);
    return;
  }

  STUNMessage request = requests_[key]->message.createRequest();
  QByteArray message  = requests_[key]->message.hostToNetwork(request);

  requests_[key]->message.cacheRequest(request);

  // udp_ records localport when binding so we don't hate specify it here
  if(!requests_[key]->udp.sendData(message, localAddress,
                                   serverAddress, serverPort))
  {
    requests_.erase(key);
  }
}


void NetworkCandidates::processSTUNReply(const QNetworkDatagram& packet)
{
  if(packet.data().size() < 20)
  {
    Logger::getLogger()->printDebug(DEBUG_WARNING, "STUN",
        "Received too small response to STUN query!");
    return;
  }

  QString key = packet.destinationAddress().toString() + ":"
      + QString::number(packet.destinationPort());

  if (requests_.find(key) == requests_.end())
  {
    Logger::getLogger()->printWarning(this, "Got stun request to an interface "
                                             "which has not sent one!",
                                      {"Interface"}, 
                                      {packet.destinationAddress().toString() + ":" +
                                      QString::number(packet.destinationPort())});
    return;
  }

  QByteArray data = packet.data();

  QString message = QString::fromStdString(data.toHex().toStdString());
  STUNMessage response;
  if (!requests_[key]->message.networkToHost(data, response))
  {
    return;
  }

  if (!requests_[key]->message.validateStunResponse(response))
  {
    Logger::getLogger()->printWarning(this, "Invalid STUN response from server!", 
                                      {"Message"}, {message});
    return;
  }

  std::pair<QHostAddress, uint16_t> addressInfo;

  if (response.getXorMappedAddress(addressInfo))
  {
    createSTUNCandidate(packet.destinationAddress(), packet.destinationPort(),
                        addressInfo.first, addressInfo.second);
  }
  else
  {
    Logger::getLogger()->printError(this, "STUN response sent by server was not Xor-mapped! "
                                          "Discarding...");
  }

  requests_[key]->finished = true;

  //Logger::getLogger()->printNormal(this, "STUN reply processed");
}


// Tries to bind to port and send a UDP packet just to check
// if it is worth including in candidates
bool NetworkCandidates::sanityCheck(QHostAddress interface, uint16_t port)
{
  QUdpSocket testSocket;

  if (!testSocket.bind(interface, port))
  {
    Logger::getLogger()->printNormal(this, "Could not bind to socket. Not adding to candidates", 
                                    {"Port"}, {interface.toString() + ":" + QString::number(port)});
    return false;
  }
  QByteArray data = QString("TestString").toLatin1();
  QNetworkDatagram datagram = QNetworkDatagram(data, interface, port + 1);
  datagram.setSender(interface, port);

  if (testSocket.writeDatagram(datagram) < 0)
  {
    testSocket.abort();
    Logger::getLogger()->printNormal(this, "Could not send data with socket. "
                                           "Not adding to candidates", 
                                    {"Port"}, {interface.toString() + ":" + 
                                               QString::number(port)});
    return false;
  }

  testSocket.abort();

  return true;
}
