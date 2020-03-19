#pragma once

#include "sdptypes.h"

#include <QStringList>
#include <QMutex>

#include <deque>

// This class handles the port management for each candidate of ICE.

class NetworkCandidates
{
public:
  NetworkCandidates();

  void setPortRange(uint16_t minport, uint16_t maxport, uint16_t maxRTPConnections);

  bool enoughFreePorts() const
  {
    return remainingPorts_ >= 4;
  }

  // return the lower port of the pair and removes both from list of available ports

  // TODO: update this to be based on sessionID as well so we can release the ports
  uint16_t nextAvailablePortPair();
  void makePortPairAvailable(uint16_t lowerPort);

  // allocate contiguous port range
  uint16_t allocateMediaPorts();
  void deallocateMediaPorts(uint16_t start);

private:
  uint16_t remainingPorts_;

  QMutex portLock_;
  //keeps a list of all available ports. Has only every other port because of rtcp
  std::deque<uint16_t> availablePorts_;
};
