#pragma once

#include "stun.h"
#include "sdptypes.h"

#include <QStringList>
#include <QMutex>

#include <deque>

// This class handles the reservation of ports for ICE candidates.

class NetworkCandidates : public QObject
{
  Q_OBJECT
public:
  NetworkCandidates();

  void setPortRange(uint16_t minport, uint16_t maxport, uint16_t maxRTPConnections);

  bool enoughFreePorts() const
  {
    return remainingPorts_ >= 4;
  }

  std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> localCandidates(uint8_t streams,
                                                            uint32_t sessionID);
  std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> globalCandidates(uint8_t streams,
                                                            uint32_t sessionID);
  std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> stunCandidates(uint8_t streams,
                                                            uint32_t sessionID);
  std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> turnCandidates(uint8_t streams,
                                                            uint32_t sessionID);

  void cleanupSession(uint32_t sessionID);

public slots:

  void createSTUNCandidate(QHostAddress local, quint16 localPort,
                           QHostAddress stun, quint16 stunPort);
private:

  // return the lower port of the pair and removes both from list of available ports

  // TODO: update this to be based on sessionID as well so we can release the ports
  uint16_t nextAvailablePortPair();
  void makePortPairAvailable(uint16_t lowerPort);

  // allocate contiguous port range
  uint16_t allocateMediaPorts();
  void deallocateMediaPorts(uint16_t start);

  bool isPrivateNetwork(const QString &address);

  Stun stun_;
  QHostAddress stunAddress_;

  uint16_t remainingPorts_;

  QMutex portLock_;
  //keeps a list of all available ports. Has only every other port because of rtcp
  std::deque<uint16_t> availablePorts_;
};
