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
  // all but nominatedPair are freed in handleEndOfNomination()
  QList<ICEPair *> *pairs;

  std::pair<ICEPair *, ICEPair *> nominatedPair;

  bool connectionNominated;
};

class ICE : public QObject
{
  Q_OBJECT

  public:
    ICE();
    ~ICE();

    // generate a list of local candidates for media streaming
    QList<ICEInfo *> generateICECandidates();

    // TODO
    void startNomination(QList<ICEInfo *>& local, QList<ICEInfo *>& remote, uint32_t sessionID);

    // TODO
    void respondToNominations(QList<ICEInfo *>& local, QList<ICEInfo *>& remote, uint32_t sessionID);

    // get nominated ICE pair using sessionID
    std::pair<ICEPair *, ICEPair *> getNominated(uint32_t sessionID);

    // caller must call this function to check if ICE has finished
    // sessionID must given so ICE can know which ongoing nomination should be checked
    bool callerConnectionNominated(uint32_t sessionID);

    // callee should call this function to check if ICE has finished
    // sessionID must given so ICE can know which ongoing nomination should be checked
    bool calleeConnectionNominated(uint32_t sessionID);

  public slots:
    // when FlowControllee has finished its job, it emits "ready" signal which is caught by this slot function
    // handleCallerEndOfNomination() check if the nomination succeeed, saves the nominated pair to hashmap and
    // releases caller_mtx to signal that negotiation is done
    void handleCallerEndOfNomination(struct ICEPair *candidateRTP, struct ICEPair *candidateRTCP, uint32_t sessionID);

    // when FlowController has finished its job, it emits "ready" signal which is caught by this slot function
    // handleCalleeEndOfNomination() check if the nomination succeeed, saves the nominated pair to hashmap and
    // releases callee_mtx to signal that negotiation is done
    void handleCalleeEndOfNomination(struct ICEPair *candidateRTP, struct ICEPair *candidateRTCP, uint32_t sessionID);
    void createSTUNCandidate(QHostAddress address);

  private:
    void handleEndOfNomination(struct ICEPair *candidateRTP, struct ICEPair *candidateRTCP, uint32_t sessionID);

    int calculatePriority(int type, int local, int component);
    QString generateFoundation();
    void printCandidate(ICEInfo *candidate);
    QList<ICEPair *> *makeCandiatePairs(QList<ICEInfo *>& local, QList<ICEInfo *>& remote);

    uint16_t portPair;
    bool nominatingConnection_;
    bool iceDisabled_;

    Stun stun_;

    ICEInfo *stun_entry_rtp_;
    ICEInfo *stun_entry_rtcp_;

    QMap<uint32_t, struct nominationInfo> nominationInfo_;

    SDPParameterManager parameters_;
};
