#pragma once

#include <QHostAddress>
#include <QString>
#include <QWaitCondition>
#include <memory>

#include "stun.h"
#include "sdptypes.h"
#include "icetypes.h"
#include "sdpparametermanager.h"

class FlowController;
class FlowControllee;

struct nominationInfo
{
  FlowControllee *controllee;
  FlowController *controller;

  // list of all candidates, remote and local
  QList<std::shared_ptr<ICEPair>> pairs;

  std::pair<
    std::shared_ptr<ICEPair>,
    std::shared_ptr<ICEPair>
  > nominatedVideo;

  std::pair<
    std::shared_ptr<ICEPair>,
    std::shared_ptr<ICEPair>
  > nominatedAudio;

  bool connectionNominated;
};

class ICE : public QObject
{
  Q_OBJECT

  public:
    ICE();
    ~ICE();

    // generate a list of local candidates for media streaming
    QList<std::shared_ptr<ICEInfo>> generateICECandidates();

    // Callee calls this function to start the connectivity check/nomination process when the 200 OK SDP has been sent to remote
    // startNomination() spawns a FlowController thread which is responsible for handling connectivity checks and nomination.
    //
    // When FlowController has finished (succeeed or failed), it sends a ready() signal which is caught by handleCalleeEndOfNomination slot
    void startNomination(QList<std::shared_ptr<ICEInfo>>& local, QList<std::shared_ptr<ICEInfo>>& remote, uint32_t sessionID);

    // When caller receives the 200 OK SDP, it should call this function to start the ICE process. respondToNominations() spawns
    // a FlowControllee thread which in turn spawns ConnectionTester threads.
    //
    // When FlowController is finished, it sends ready() signal is is caught by handleCallerEndOfNomination slot
    void respondToNominations(QList<std::shared_ptr<ICEInfo>>& local, QList<std::shared_ptr<ICEInfo>>& remote, uint32_t sessionID);

    // get nominated ICE pair using sessionID
    ICEMediaInfo getNominated(uint32_t sessionID);

    // free all ICE-related resources
    void cleanupSession(uint32_t sessionID);

signals:
    void nominationFailed(quint32 sessionID);
    void nominationSucceeded(quint32 sessionID);

  public slots:
    // when FlowControllee has finished its job, it emits "ready" signal which is caught by this slot function
    // handleCallerEndOfNomination() check if the nomination succeeed, saves the nominated pair to hashmap and
    // releases caller_mtx to signal that negotiation is done
    void handleCallerEndOfNomination(std::shared_ptr<ICEPair> rtp, std::shared_ptr<ICEPair> rtcp, uint32_t sessionID);

    // when FlowController has finished its job, it emits "ready" signal which is caught by this slot function
    // handleCalleeEndOfNomination() check if the nomination succeeed, saves the nominated pair to hashmap and
    // releases callee_mtx to signal that negotiation is done
    void handleCalleeEndOfNomination(std::shared_ptr<ICEPair> rtp, std::shared_ptr<ICEPair> rtcp, uint32_t sessionID);

    void createSTUNCandidate(QHostAddress address);

  private:
    // create media candidate (RTP and RTCP connection)
    // "type" marks whether this candidate is host or server reflexive candidate (affects priority)
    std::pair<std::shared_ptr<ICEInfo>, std::shared_ptr<ICEInfo>> makeCandidate(QHostAddress address, QString type);

    // save nominated pair to hashmap so it can be fetched later on
    void handleEndOfNomination(std::shared_ptr<ICEPair> rtp, std::shared_ptr<ICEPair> rtcp, uint32_t sessionID);

    int calculatePriority(int type, int local, int component);

    void printCandidate(ICEInfo *candidate);

    bool isPrivateNetwork(const QHostAddress& address);

    /* Check the status of ICE from settings and adjust iceEnabled_ accordingly */
    void checkICEstatus();

    // makeCandidatePairs takes a list of local and remote candidates, matches them based on localilty (host/server-reflexive)
    // and component (RTP/RTCP) and returns a list of ICEPairs used for connectivity checks
    QList<std::shared_ptr<ICEPair>> makeCandidatePairs(QList<std::shared_ptr<ICEInfo>>& local, QList<std::shared_ptr<ICEInfo>>& remote);

    bool nominatingConnection_;
    bool iceEnabled_;

    Stun stun_;
    QHostAddress stunAddress_;
    SDPParameterManager parameters_;

    QMap<uint32_t, struct nominationInfo> nominationInfo_;
};
