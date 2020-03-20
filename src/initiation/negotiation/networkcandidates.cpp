#include "networkcandidates.h"

#include "common.h"

#include <QNetworkInterface>
#include <QUdpSocket>
#include <QDebug>


const uint16_t STUN_PORT       = 21000;

NetworkCandidates::NetworkCandidates()
: stun_(),
  stunAddress_(QHostAddress(""))
{}

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
      availablePorts_.insert(std::pair<QString, std::deque<uint16_t>>(address.toString(),{}));

      for(uint16_t i = minport; i < maxport; i += 2)
      {
        makePortPairAvailable(address.toString(), i);
      }
    }
  }

  QObject::connect( &stun_, &Stun::stunAddressReceived,
                    this,  &NetworkCandidates::createSTUNCandidate);

  // TODO: Probably best way to do this is periodically every 10 minutes or so.
  // That way we get our current STUN address
  stun_.wantAddress("stun.l.google.com", STUN_PORT);
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

  // TODO: Even though unlikely, this should probably be prepared for
  // multiple addresses.
  stunAddress_ = stun;
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

  printDebug(DEBUG_NORMAL, "SDP Parameter Manager",
             "Binding finished", {"Bound lower port"}, {QString::number(newLowerPort)});

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
