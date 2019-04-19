#pragma once

#include <QHostAddress>
#include <QString>
#include <QWaitCondition>
#include <QMutex>
#include <memory>

#include "stun.h"
#include "sip/sdptypes.h"
#include "icetypes.h"
#include "sdpparametermanager.h"

class FlowController;
class FlowControllee;

struct nominationInfo
{
  QMutex *caller_mtx;
  QMutex *callee_mtx;

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

    // TODO
    void startNomination(QList<std::shared_ptr<ICEInfo>>& local, QList<std::shared_ptr<ICEInfo>>& remote, uint32_t sessionID);

    // TODO
    void respondToNominations(QList<std::shared_ptr<ICEInfo>>& local, QList<std::shared_ptr<ICEInfo>>& remote, uint32_t sessionID);

    // get nominated ICE pair using sessionID
    /* std::pair<ICEPair *, ICEPair *> getNominated(uint32_t sessionID); */
    ICEMediaInfo getNominated(uint32_t sessionID);

    // caller must call this function to check if ICE has finished
    // sessionID must given so ICE can know which ongoing nomination should be checked
    bool callerConnectionNominated(uint32_t sessionID);

    // callee should call this function to check if ICE has finished
    // sessionID must given so ICE can know which ongoing nomination should be checked
    bool calleeConnectionNominated(uint32_t sessionID);

    // free all ICE-related resources
    void cleanupSession(uint32_t sessionID);

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

    void handleEndOfNomination(std::shared_ptr<ICEPair> rtp, std::shared_ptr<ICEPair> rtcp, uint32_t sessionID);

    int calculatePriority(int type, int local, int component);
    QString generateFoundation();
    void printCandidate(ICEInfo *candidate);

    QList<std::shared_ptr<ICEPair>> makeCandidatePairs(QList<std::shared_ptr<ICEInfo>>& local, QList<std::shared_ptr<ICEInfo>>& remote);

    bool nominatingConnection_;
    bool iceDisabled_;

    Stun stun_;
    QHostAddress stunAddress_;
    SDPParameterManager parameters_;

    QMap<uint32_t, struct nominationInfo> nominationInfo_;
};
