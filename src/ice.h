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

    QList<ICEInfo *> generateICECandidates(QList<ICEInfo *> *remoteCandidates);

    void addRemoteCandidate(const ICEInfo *candidate);
    bool connectionNominated();

    // TODO
    void startNomination(QList<ICEInfo *>& local, QList<ICEInfo *>& remote);

    // TODO
    void respondToNominations(QList<ICEInfo *>& local, QList<ICEInfo *>& remote);

    // TODO
    std::pair<ICEPair *, ICEPair *> getNominated();

  public slots:
    void handleEndOfNomination(struct ICEPair *candidateRTP, struct ICEPair *candidateRTCP);
    void createSTUNCandidate(QHostAddress address);

  private:
    int calculatePriority(int type, int local, int component);
    /* TODO: createCandidate */
    QString generateFoundation();
    void printCandidate(ICEInfo *candidate);
    QList<ICEPair *> *makeCandiatePairs(QList<ICEInfo *>& local, QList<ICEInfo *>& remote);

    uint16_t portPair;
    bool connectionNominated_;
    bool nominatingConnection_;

    Stun stun_;

    QMutex nominating_mtx;
    QWaitCondition nominated_cond;

    FlowController *caller_;

    ICEInfo *stun_entry_rtp_;
    ICEInfo *stun_entry_rtcp_;

    ICEPair *nominated_rtp;
    ICEPair *nominated_rtcp;
};
