#pragma once

#include <QHostAddress>
#include <QString>
#include <QWaitCondition>
#include <QMutex>

#include "stun.h"
#include "sip/sdptypes.h"

enum PAIR {
  PAIR_WAITING     = 0,
  PAIR_IN_PROGRESS = 1,
  PAIR_SUCCEEDED   = 2,
  PAIR_FAILED      = 3,
  PAIR_FROZEN      = 4,
};

enum COMPONENTS {
  RTP  = 1,
  RTCP = 2
};


struct ICEPair
{
  struct ICEInfo *local;
  struct ICEInfo *remote;
  int priority;
  int state;
};

#include "iceflowcontrol.h"

class ICE : public QObject
{
  Q_OBJECT

  public:
    ICE();
    ~ICE();

    QList<ICEInfo *> generateICECandidates();

    void addRemoteCandidate(const ICEInfo *candidate);
    bool connectionNominated(bool caller);

    // TODO
    void startNomination(QList<ICEInfo *>& local, QList<ICEInfo *>& remote, uint32_t sessionID);

    // TODO
    void respondToNominations(QList<ICEInfo *>& local, QList<ICEInfo *>& remote, uint32_t sessionID);

    // get nominated ICE pair using sessionID
    std::pair<ICEPair *, ICEPair *> getNominated(uint32_t sessionID);

    // caller must call this function to check if ICE has finished
    bool callerConnectionNominated();

    // callee should call this function to check if ICE has finished
    bool calleeConnectionNominated();

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
    /* TODO: createCandidate */
    QString generateFoundation();
    void printCandidate(ICEInfo *candidate);
    QList<ICEPair *> *makeCandiatePairs(QList<ICEInfo *>& local, QList<ICEInfo *>& remote);

    uint16_t portPair;
    bool connectionNominated_;
    bool nominatingConnection_;

    Stun stun_;

    // separate mutexes for caller and callee to make localhost calls work
    QMutex caller_mtx;
    QMutex callee_mtx;
    QWaitCondition nominated_cond;

    ICEInfo *stun_entry_rtp_;
    ICEInfo *stun_entry_rtcp_;

    ICEPair *nominated_rtp;
    ICEPair *nominated_rtcp;

    QMap<uint32_t, std::pair<ICEPair *, ICEPair *>> nominated_;
};
