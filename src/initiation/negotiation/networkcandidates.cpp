#include "networkcandidates.h"

#include "common.h"

#include <QNetworkInterface>
#include <QUdpSocket>
#include <QDebug>

const uint16_t GOOGLE_STUN_PORT = 19302;

const uint16_t STUNADDRESSPOOL = 5;

NetworkCandidates::NetworkCandidates():
  requests_(),
  stunAddresses_(),
  stunBindings_(),
  portLock_(),
  availablePorts_(),
  reservedPorts_()
{}


NetworkCandidates::~NetworkCandidates()
{
  requests_.clear();
}


void NetworkCandidates::setPortRange(uint16_t minport,
                                     uint16_t maxport)
{
  if(minport >= maxport)
  {
    printProgramError(this, "Min port is smaller or equal to max port");
  }

  foreach (const QHostAddress& address, QNetworkInterface::allAddresses())
  {
    if (address.protocol() == QAbstractSocket::IPv4Protocol &&
        !address.isLoopback())
    {
      if (sanityCheck(address, minport))
      {
        availablePorts_.insert(std::pair<QString, std::deque<uint16_t>>(address.toString(),{}));

        for(uint16_t i = minport; i < maxport; i += 2)
        {
          makePortPairAvailable(address.toString(), i);
        }
      }
    }
  }

  // TODO: Probably best way to do this is periodically every 10 minutes or so.
  // That way we get our current STUN address
  wantAddress("stun.l.google.com");
}


void NetworkCandidates::createSTUNCandidate(QHostAddress local, quint16 localPort,
                                            QHostAddress stun, quint16 stunPort)
{
  if (stun == QHostAddress(""))
  {
    printDebug(DEBUG_WARNING, "ICE",
       "Failed to resolve public IP! Server-reflexive candidates won't be created!");
    return;
  }

  printDebug(DEBUG_NORMAL, this, "Created ICE STUN candidate", {"LocalAddress", "STUN address"},
            {local.toString() + ":" + QString::number(localPort),
             stun.toString() + ":" + QString::number(stunPort)});

  stunAddresses_.push_back({stun, stunPort});
  stunBindings_.push_back({local, localPort});

  //moreSTUNCandidates();
}


std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> NetworkCandidates::localCandidates(
    uint8_t streams, uint32_t sessionID)
{
  std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> addresses
      =   std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> (new QList<std::pair<QHostAddress, uint16_t>>());

  for (auto& interface : availablePorts_)
  {
    if (isPrivateNetwork(interface.first))
    {
      addresses->push_back({QHostAddress(interface.first),
                            nextAvailablePortPair(interface.first, sessionID)});
    }
  }

  return addresses;
}


std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> NetworkCandidates::globalCandidates(
    uint8_t streams, uint32_t sessionID)
{
  std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> addresses
      =   std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> (new QList<std::pair<QHostAddress, uint16_t>>());

  for (auto& interface : availablePorts_)
  {
    if (!isPrivateNetwork(interface.first))
    {
      addresses->push_back({QHostAddress(interface.first),
                            nextAvailablePortPair(interface.first, sessionID)});
    }
  }
  return addresses;
}


std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> NetworkCandidates::stunCandidates(
    uint8_t streams, uint32_t sessionID)
{
  std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> addresses
      =   std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> (new QList<std::pair<QHostAddress, uint16_t>>());
  if (!stunAddresses_.empty())
  {
    addresses->push_back(stunAddresses_.front());
    stunAddresses_.pop_front();

    // TODO: Move reservedports reservation from stun to sessionID

    //moreSTUNCandidates();
  }
  else
  {
    printWarning(this, "No STUN candidates found!");
  }

  return addresses;
}


std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> NetworkCandidates::stunBindings(
    uint8_t streams, uint32_t sessionID)
{
  std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> addresses
      =   std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> (new QList<std::pair<QHostAddress, uint16_t>>());

  if (!stunBindings_.empty())
  {
    addresses->push_back(stunBindings_.front());
    stunBindings_.pop_front();
  }
  else
  {
    printWarning(this, "No STUN bindings added!");
  }

  return addresses;
}


std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> NetworkCandidates::turnCandidates(
    uint8_t streams, uint32_t sessionID)
{
  Q_UNUSED(streams);
  Q_UNUSED(sessionID);

  // We are probably never going to support TURN addresses.
  // If we are, add candidates to the list.
  std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> addresses
      =   std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> (new QList<std::pair<QHostAddress, uint16_t>>());
  return addresses;
}


uint16_t NetworkCandidates::nextAvailablePortPair(QString interface, uint32_t sessionID)
{


  uint16_t newLowerPort = 0;

  portLock_.lock();

  if (availablePorts_.find(interface) == availablePorts_.end() ||
      availablePorts_[interface].size() == 0)
  {
    portLock_.unlock();
    printWarning(this, "Either couldn't find interface or "
                       "this interface has run out of ports");
    return 0;
  }

  newLowerPort = availablePorts_[interface].at(0);
  availablePorts_[interface].pop_front();
  reservedPorts_[sessionID].push_back(std::pair<QString, uint16_t>(interface, newLowerPort));

  // This is because of a hack in ICE which uses upper ports for opus instead of allocating new ones
  // TODO: Remove once ice supports any amount of media streams.
  availablePorts_[interface].pop_front();
  reservedPorts_[sessionID].push_back(std::pair<QString, uint16_t>(interface, newLowerPort + 2));

  /*
  // TODO: I'm suspecting this may sometimes hang Kvazzup at the start
  if(availablePorts_.size() >= 1)
  {
    QUdpSocket test_port1;
    QUdpSocket test_port2;
    do
    {
      newLowerPort = availablePorts_.at(0);
      availablePorts_.pop_front();
      qDebug() << "Trying to bind ports:" << newLowerPort << "and" << newLowerPort + 1;

    } while(!test_port1.bind(newLowerPort) && !test_port2.bind(newLowerPort + 1));
    test_port1.abort();
    test_port2.abort();
  }
  else
  {
    qDebug() << "Could not reserve ports. Ports available:" << availablePorts_.size();
  }
  */
  portLock_.unlock();

  //printDebug(DEBUG_NORMAL, "SDP Parameter Manager",
  //           "Binding finished", {"Bound lower port"}, {QString::number(newLowerPort)});

  return newLowerPort;
}

void NetworkCandidates::makePortPairAvailable(QString interface, uint16_t lowerPort)
{


  if(lowerPort != 0)
  {
    portLock_.lock();
    if (availablePorts_.find(interface) == availablePorts_.end())
    {
      portLock_.unlock();
      printWarning(this, "Couldn't find interface when making ports available.");
      return;
    }
    //qDebug() << "Freed ports:" << lowerPort << "/" << lowerPort + 1;

    availablePorts_[interface].push_back(lowerPort);
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
    printWarning(this, "Tried to cleanup session with no reserved ports");
    return;
  }

  for(auto& session : reservedPorts_[sessionID])
  {
    makePortPairAvailable(session.first, session.second);
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
    printProgramError(this, "Interfaces have not been set before sending STUN requests");
    return;
  }

  const auto addresses = info.addresses();
  if(addresses.size() != 0)
  {
    stunServerAddress_ = addresses.at(0);
  }
  else
  {
    return;
  }

  // Send STUN-Request through all the interfaces, since we don't know which is
  // connected to the internet. Most of them will fail.

  moreSTUNCandidates();
}

void NetworkCandidates::moreSTUNCandidates()
{
  if (!stunServerAddress_.isNull())
  {
    if(stunAddresses_.size() < STUNADDRESSPOOL)
    {
      for (auto& interface : availablePorts_)
      {
        // use 0 as STUN sessionID
        sendSTUNserverRequest(QHostAddress(interface.first), nextAvailablePortPair(interface.first, 0),
                              stunServerAddress_,                 GOOGLE_STUN_PORT);
      }
    }
  }
  else
  {
    printProgramError(this, "STUN server address not set!");
  }
}


void NetworkCandidates::sendSTUNserverRequest(QHostAddress localAddress,
                                              uint16_t localPort,
                                              QHostAddress serverAddress,
                                              uint16_t serverPort)
{
  if (localPort == 0 || serverPort == 0)
  {
    printProgramError(this, "Not port set. Can't get STUN address.");
    return;
  }

  printNormal(this, "Sending STUN server request", {"Path"},
          {localAddress.toString() + ":" + QString::number(localPort) + " -> " +
           serverAddress.toString() + ":" + QString::number(serverPort)});

  if (requests_.find(localAddress.toString()) == requests_.end())
  {
    requests_[localAddress.toString()] = std::shared_ptr<STUNRequest>(new STUNRequest);
  }

  requests_[localAddress.toString()]->udp.bindSocket(localAddress, localPort);

  QObject::connect(&requests_[localAddress.toString()]->udp, &UDPServer::datagramAvailable,
                   this,              &NetworkCandidates::processSTUNReply);

  STUNMessage request = requests_[localAddress.toString()]->message.createRequest();
  QByteArray message  = requests_[localAddress.toString()]->message.hostToNetwork(request);

  requests_[localAddress.toString()]->message.cacheRequest(request);

  // udp_ records localport when binding so we don't hate specify it here
  requests_[localAddress.toString()]->udp.sendData(message, localAddress,
                                                   serverAddress, serverPort);
}


void NetworkCandidates::processSTUNReply(const QNetworkDatagram& packet)
{
  if(packet.data().size() < 20)
  {
    printDebug(DEBUG_WARNING, "STUN",
        "Received too small response to STUN query!");
    return;
  }

  QString destinationAddress = packet.destinationAddress().toString();

  if (requests_.find(destinationAddress) == requests_.end())
  {
    printWarning(this, "Got stun request to an interface which has not sent one!",
      {"Interface"}, {packet.destinationAddress().toString() + ":" + QString::number(packet.destinationPort())});
    return;
  }

  QByteArray data = packet.data();

  QString message = QString::fromStdString(data.toHex().toStdString());
  STUNMessage response = requests_[destinationAddress]->message.networkToHost(data);
  // free socket for further use


  if (!requests_[destinationAddress]->message.validateStunResponse(response))
  {
    printWarning(this, "Invalid STUN response from server!", {"Message"}, {message});
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
    printError(this, "STUN response sent by server was not Xor-mapped! Discarding...");
  }


  // TODO: Modified after it was freed. Probably because the freed socket sent this packet
  //requests_[destinationAddress]->udp.unbind();

  printNormal(this, "STUN reply processed");
}


// Tries to bind to port and send a UDP packet just to check
// if it is worth including in candidates
bool NetworkCandidates::sanityCheck(QHostAddress interface, uint16_t port)
{
  QUdpSocket testSocket;

  if (!testSocket.bind(interface, port))
  {
    printNormal(this, "Could not bind to socket. Not adding to candidates", {"Port"}, {
                  interface.toString() + ":" + QString::number(port)});
    return false;
  }
  QByteArray data = QString("TestString").toLatin1();
  QNetworkDatagram datagram = QNetworkDatagram(data, interface, port + 1);
  datagram.setSender(interface, port);

  if (testSocket.writeDatagram(datagram) < 0)
  {
    testSocket.abort();
    printNormal(this, "Could not send data with socket. Not adding to candidates", {"Port"}, {
                  interface.toString() + ":" + QString::number(port)});
    return false;
  }

  testSocket.abort();

  return true;
}

