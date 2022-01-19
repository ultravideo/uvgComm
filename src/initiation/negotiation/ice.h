#pragma once

#include "networkcandidates.h"

#include "initiation/sipmessageprocessor.h"

#include "sdptypes.h"
#include "icetypes.h"

#include <QHostAddress>
#include <QString>
#include <QWaitCondition>

#include <memory>

class IceSessionTester;

class ICE : public SIPMessageProcessor
{
  Q_OBJECT

  public:
  ICE(std::shared_ptr<NetworkCandidates> candidates,
      uint32_t sessionID);
  ~ICE();

  // free all ICE-related resources
  void uninit();


public slots:

  virtual void processOutgoingRequest(SIPRequest& request, QVariant& content);
  virtual void processOutgoingResponse(SIPResponse& response, QVariant& content);

  virtual void processIncomingRequest(SIPRequest& request, QVariant& content);
  virtual void processIncomingResponse(SIPResponse& response, QVariant& content);

signals:
  void nominationFailed(quint32 sessionID);
  void nominationSucceeded(QList<std::shared_ptr<ICEPair>>& streams,
                           quint32 sessionID);

private slots:
  // saves the nominated pair so it can be fetched later on and
  // sends nominationSucceeded signal that negotiation is done
  void handeICESuccess(QList<std::shared_ptr<ICEPair>> &streams);

  // ends testing and emits nominationFailed
  void handleICEFailure();

private:

  // Adds local ICE candidates to SDP and starts nomination if we have
  // their candidates.
  void addLocalStartNomination(QVariant& content);

  // Takes remote ICE candidates from SDP and start nomination if we have
  // sent local candidates.
  void takeRemoteStartNomination(QVariant& content);

  // generate a list of local candidates for media streaming
  QList<std::shared_ptr<ICEInfo>>
      generateICECandidates(std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> localCandidates,
                            std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> globalCandidates,
                            std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> stunCandidates,
                            std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> stunBindings,
                            std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> turnCandidates);

  // Call this function to start the connectivity check/nomination process.
  // The other side should start negotiation as fast as possible
  // Does not block
  void startNomination(QList<std::shared_ptr<ICEInfo>>& local,
                       QList<std::shared_ptr<ICEInfo>>& remote, bool controller);




  // create one component candidate
  std::shared_ptr<ICEInfo> makeCandidate(uint32_t foundation,
                                         CandidateType type,
                                         uint8_t component,
                                         const QHostAddress address,
                                         quint16 port,
                                         const QHostAddress relayAddress,
                                         quint16 relayPort,
                                         quint16 localPriority);

  // calculate priority as recommended by specification
  int candidateTypePriority(CandidateType type, quint16 local, uint8_t component) const;
  int pairPriority(int controllerCandidatePriority, int controlleeCandidatePriority) const;

  void printCandidates(QList<std::shared_ptr<ICEInfo>>& candidates);

  // Takes a list of local and remote candidates, matches them based component
  // and returns a list of all possible ICEPairs used for connectivity checks
  QList<std::shared_ptr<ICEPair>> makeCandidatePairs(QList<std::shared_ptr<ICEInfo>>& local,
                                                     QList<std::shared_ptr<ICEInfo>>& remote, bool controller);

  // creates component candidates for this address list
  void addCandidates(std::shared_ptr<QList<std::pair<QHostAddress,
                     uint16_t>>> addresses,
                     std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> relayAddresses,
                     quint32 &foundation,
                     CandidateType type, quint16 localPriority,
                     QList<std::shared_ptr<ICEInfo>>& candidates);

  void addICEToSupported(std::shared_ptr<QStringList> &supported);

  bool isICEToSupported(std::shared_ptr<QStringList> supported);

  std::shared_ptr<NetworkCandidates> networkCandidates_;
  uint32_t sessionID_;

  // information related to one nomination process

  std::shared_ptr<IceSessionTester> agent_;

  QList<std::shared_ptr<ICEInfo>> localCandidates_;
  QList<std::shared_ptr<ICEInfo>> remoteCandidates_;

  // list of all candidates, remote and local
  QList<std::shared_ptr<ICEPair>> pairs_;

  bool connectionNominated_;

  bool peerSupportsICE_;
};
