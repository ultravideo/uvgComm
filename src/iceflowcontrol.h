#pragma once

#include <QWaitCondition>
#include <QMutex>
#include <QThread>
#include <QList>

#include "stun.h"
#include "icetypes.h"

class FlowAgent : public QThread
{
  Q_OBJECT

public:
  FlowAgent();
  ~FlowAgent();
  void setCandidates(QList<ICEPair *> *candidates);
  void setSessionID(uint32_t sessionID);
  bool waitForResponses(unsigned long timeout);

signals:
  void ready(ICEPair *candidateRTP, ICEPair *candidateRTCP, uint32_t sessionID);
  void endNomination();

public slots:
  void nominationDone(ICEPair *connection);

protected:
    void run();

    QList<ICEPair *> *candidates_;
    uint32_t sessionID_;

    // temporary storage for succeeded pairs, when both RTP and RTCP
    // of some candidate pair succeeds, endNomination() signal is emitted
    // and the succeeded pair is copied to nominated_rtp_ and nominated_rtcp_
    //
    // the first candidate pair that has both RTP and RTCP tested is chosen
    QMap<QString, std::pair<ICEPair *, ICEPair *>> nominated_;

    ICEPair *nominated_rtp_;
    ICEPair *nominated_rtcp_;
    QMutex nominated_mtx;
};

class FlowController : public FlowAgent
{
  Q_OBJECT

protected:
    void run();
};

class FlowControllee : public FlowAgent
{
  Q_OBJECT

protected:
    void run();
};
