#include "networkcandidates.h"

#include "common.h"

#include <QNetworkInterface>
#include <QUdpSocket>
#include <QDebug>


const uint16_t STUN_PORT       = 21000;

NetworkCandidates::NetworkCandidates()
: stun_(),
  stunAddress_(QHostAddress("")),
  remainingPorts_(0)
{}

void NetworkCandidates::setPortRange(uint16_t minport,
                                     uint16_t maxport,
                                     uint16_t maxRTPConnections)
{
  if(minport >= maxport)
  {
    qDebug() << "ERROR: Min port is smaller or equal to max port for SDP";
  }

  for(uint16_t i = minport; i < maxport; i += 2)
  {
    makePortPairAvailable(i);
  }

  remainingPorts_ = maxRTPConnections;

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
  foreach (const QHostAddress& address, QNetworkInterface::allAddresses())
  {
    if (address.protocol() == QAbstractSocket::IPv4Protocol &&
        isPrivateNetwork(address.toString()))
    {
      addresses->push_back({address, allocateMediaPorts()});
    }
  }
  return addresses;
}


std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> NetworkCandidates::globalCandidates(
    uint8_t streams, uint32_t sessionID)
{
  std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> addresses
      =   std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> (new QList<std::pair<QHostAddress, uint16_t>>());

  foreach (const QHostAddress& address, QNetworkInterface::allAddresses())
  {
    if (address.protocol() == QAbstractSocket::IPv4Protocol &&
        !isPrivateNetwork(address.toString()))
    {
      addresses->push_back({address, allocateMediaPorts()});
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


uint16_t NetworkCandidates::allocateMediaPorts()
{
  if (remainingPorts_ < 4)
  {
    qDebug() << "ERROR: Not enough free ports, remaining:" << remainingPorts_;
    return 0;
  }

  portLock_.lock();

  uint16_t start = availablePorts_.at(0);

  availablePorts_.pop_front();
  availablePorts_.pop_front();
  remainingPorts_ -= 4;

  portLock_.unlock();

  return start;
}

void NetworkCandidates::deallocateMediaPorts(uint16_t start)
{
  portLock_.lock();

  availablePorts_.push_back(start);
  availablePorts_.push_back(start + 2);
  remainingPorts_ += 4;

  portLock_.unlock();
}

uint16_t NetworkCandidates::nextAvailablePortPair()
{
  // TODO: I'm suspecting this may sometimes hang Kvazzup at the start

  uint16_t newLowerPort = 0;

  portLock_.lock();
  if(availablePorts_.size() >= 1 && remainingPorts_ >= 2)
  {
    QUdpSocket test_port1;
    QUdpSocket test_port2;
    do
    {
      newLowerPort = availablePorts_.at(0);
      availablePorts_.pop_front();
      remainingPorts_ -= 2;
      qDebug() << "Trying to bind ports:" << newLowerPort << "and" << newLowerPort + 1;

    } while(!test_port1.bind(newLowerPort) && !test_port2.bind(newLowerPort + 1));
    test_port1.abort();
    test_port2.abort();
  }
  else
  {
    qDebug() << "Could not reserve ports. Remaining ports:" << remainingPorts_
             << "deque size:" << availablePorts_.size();
  }
  portLock_.unlock();

  printDebug(DEBUG_NORMAL, "SDP Parameter Manager",
             "Binding finished", {"Bound lower port"}, {QString::number(newLowerPort)});

  return newLowerPort;
}

void NetworkCandidates::makePortPairAvailable(uint16_t lowerPort)
{
  if(lowerPort != 0)
  {
    //qDebug() << "Freed ports:" << lowerPort << "/" << lowerPort + 1
    //         << "Ports available:" << remainingPorts_;
    portLock_.lock();
    availablePorts_.push_back(lowerPort);
    remainingPorts_ += 2;
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
