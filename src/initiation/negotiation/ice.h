#pragma once

#include "sdptypes.h"
#include "icetypes.h"
#include "networkcandidates.h"

#include <QHostAddress>
#include <QString>
#include <QWaitCondition>

#include <memory>

class IceSessionTester;

class ICE : public QObject
{
  Q_OBJECT

  public:
    ICE();
    ~ICE();

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
                         QList<std::shared_ptr<ICEInfo>>& remote,
                         uint32_t sessionID, bool flowController);

    // get nominated ICE pairs for sessionID
    QList<std::shared_ptr<ICEPair> > getNominated(uint32_t sessionID);

    // free all ICE-related resources for sessionID
    void cleanupSession(uint32_t sessionID);

signals:
    void nominationFailed(quint32 sessionID);
    void nominationSucceeded(quint32 sessionID);

private slots:
    // saves the nominated pair so it can be fetched later on and
    // sends nominationSucceeded signal that negotiation is done
    void handeICESuccess(QList<std::shared_ptr<ICEPair> > &streams,
                         uint32_t sessionID);

    // ends testing and emits nominationFailed
    void handleICEFailure(uint32_t sessionID);

  private:

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
    int calculatePriority(CandidateType type, quint16 local, uint8_t component);

    void printCandidates(QList<std::shared_ptr<ICEInfo>>& candidates);

    // Takes a list of local and remote candidates, matches them based component
    // and returns a list of all possible ICEPairs used for connectivity checks
    QList<std::shared_ptr<ICEPair>> makeCandidatePairs(QList<std::shared_ptr<ICEInfo>>& local,
                                                       QList<std::shared_ptr<ICEInfo>>& remote);

    // creates component candidates for this address list
    void addCandidates(std::shared_ptr<QList<std::pair<QHostAddress,
                       uint16_t>>> addresses,
                       std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> relayAddresses,
                       quint32 &foundation,
                       CandidateType type, quint16 localPriority,
                       QList<std::shared_ptr<ICEInfo>>& candidates);

    // information related to one nomination process
    struct NominationInfo
    {
      IceSessionTester *agent;

      // list of all candidates, remote and local
      QList<std::shared_ptr<ICEPair>> pairs;
      QList<std::shared_ptr<ICEPair>> selectedPairs;

      bool connectionNominated;
    };

    // key is sessionID
    QMap<uint32_t, struct NominationInfo> nominationInfo_;
};
