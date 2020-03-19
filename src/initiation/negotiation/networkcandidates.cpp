#include "networkcandidates.h"

#include "common.h"

#include <QUdpSocket>
#include <QDebug>


NetworkCandidates::NetworkCandidates()
  :remainingPorts_(0)
{}

void NetworkCandidates::setPortRange(uint16_t minport, uint16_t maxport, uint16_t maxRTPConnections)
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

