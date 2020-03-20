#pragma once

#include "stun.h"
#include "sdptypes.h"

#include <QStringList>
#include <QMutex>

#include <deque>

// This class handles the reservation of ports for ICE candidates.
struct STUNRequest
{
  UDPServer udp;
  StunMessageFactory message;
};


class NetworkCandidates : public QObject
{
  Q_OBJECT
public:
  NetworkCandidates();
  ~NetworkCandidates();

  void setPortRange(uint16_t minport, uint16_t maxport);

  std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> localCandidates(uint8_t streams,
                                                            uint32_t sessionID);
  std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> globalCandidates(uint8_t streams,
                                                            uint32_t sessionID);
  std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> stunCandidates(uint8_t streams,
                                                            uint32_t sessionID);
  std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> turnCandidates(uint8_t streams,
                                                            uint32_t sessionID);

  void cleanupSession(uint32_t sessionID);

private slots:
  void processReply(const QNetworkDatagram &packet);

  // sends STUN request through each of our interfaces
  void handleStunHostLookup(QHostInfo info);

private:
  void sendSTUNserverRequest(QHostAddress localAddress, uint16_t localPort,
                             QHostAddress serverAddress, uint16_t serverPort);
  void wantAddress(QString stunServer);
  void createSTUNCandidate(QHostAddress local, quint16 localPort,
                           QHostAddress stun, quint16 stunPort);

  // return the lower port of the pair and removes both from list of available ports
  uint16_t nextAvailablePortPair(QString interface, uint32_t sessionID);
  void makePortPairAvailable(QString interface, uint16_t lowerPort);

  bool isPrivateNetwork(const QString &address);

  QHostAddress stunAddress_;

  std::map<QString, std::shared_ptr<STUNRequest>> requests_;

  QMutex portLock_;

  // TODO: use pointers to reduce copying

  // Keeps a list of all available ports. Has only every other port because of rtcp
  // Key is the ip address of network interface.
  std::map<QString, std::deque<uint16_t>> availablePorts_;

  // indicates which stun port corresponds to which interface/port pair
  typedef std::map<uint16_t, std::pair<QString, uint16_t>> STUNEntry;

  // key is stun address
  std::map<QString, STUNEntry> stunPorts_;

  // key is sessionID, 0 is STUN
  std::map<uint32_t, QList<std::pair<QString, uint16_t>>> reservedPorts_;
};
