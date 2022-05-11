#pragma once

#include "sdptypes.h"
#include "udpserver.h"
#include "stunmessagefactory.h"

#include <QStringList>
#include <QMutex>
#include <QHostInfo>

#include <deque>
#include <set>


struct STUNRequest
{
  UDPServer udp;
  StunMessageFactory message;
  bool finished;
};

/* This class handles the reservation of ports for ICE candidates. */

class NetworkCandidates : public QObject
{
  Q_OBJECT
public:
  NetworkCandidates();
  ~NetworkCandidates();

  void init();

  std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> localCandidates(uint8_t streams,
                                                            uint32_t sessionID);
  std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> globalCandidates(uint8_t streams,
                                                            uint32_t sessionID);
  std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> stunCandidates(uint8_t streams);
  std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> stunBindings(uint8_t streams,
                                                            uint32_t sessionID);
  std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> turnCandidates(uint8_t streams,
                                                            uint32_t sessionID);

  void cleanupSession(uint32_t sessionID);

private slots:
  void processSTUNReply(const QNetworkDatagram &packet);

  // sends STUN request through each of our interfaces
  void handleStunHostLookup(QHostInfo info);

  void refreshSTUN();

private:
  bool sendSTUNserverRequest(QHostAddress localAddress, uint16_t localPort,
                             QHostAddress serverAddress, uint16_t serverPort);
  void wantAddress(QString stunServer);
  void createSTUNCandidate(QHostAddress local, quint16 localPort,
                           QHostAddress stun, quint16 stunPort);

  void moreSTUNCandidates();

  // return the lower port of the pair and removes both from list of available ports
  uint16_t nextAvailablePort(QString interface, uint32_t sessionID);
  void makePortAvailable(QString interface, uint16_t port);

  bool isPrivateNetwork(const QString &address);

  // Tries to bind to port and send a UDP packet just to check
  // if it is worth including in candidates
  bool sanityCheck(QHostAddress interface, uint16_t port);

  // IP address corresponding to the used STUN server address
  QHostAddress stunServerIP_;

  // ongoing stun requests, key is interface which sent it
  std::map<QString, std::shared_ptr<STUNRequest>> requests_;

  QMutex stunMutex_;
  // public IP addresses we have found
  std::deque<std::pair<QHostAddress, uint16_t>> stunAddresses_;
  // local ports that correspond to those addresses
  std::deque<std::pair<QHostAddress, uint16_t>> stunBindings_;

  QMutex portLock_;
  // Keeps a list of all available ports.
  // Key is the ip address of network interface.
  std::map<QString, std::deque<uint16_t>> availablePorts_;
  std::set<QString> stunFailureList_;

  // key is sessionID, key 0 is STUN
  std::map<uint32_t, QList<std::pair<QString, uint16_t>>> reservedPorts_;

  QTimer refreshSTUNTimer_;

  bool behindNAT_;

  uint16_t currentMinPort_;
  QString stunServerAddress_;
  uint16_t stunPort_;
  bool stunEnabled_;
};
