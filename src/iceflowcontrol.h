#pragma once

#include <QWaitCondition>
#include <QMutex>
#include <QThread>
#include <QList>
#include <memory>

#include "stun.h"
#include "icetypes.h"

class FlowAgent : public QThread
{
  Q_OBJECT

public:
  FlowAgent();
  ~FlowAgent();
  void setCandidates(QList<std::shared_ptr<ICEPair>> *candidates);
  void setSessionID(uint32_t sessionID);
  bool waitForResponses(unsigned long timeout);

signals:
  void ready(std::shared_ptr<ICEPair> candidateRTP, std::shared_ptr<ICEPair> candidateRTCP, uint32_t sessionID);
  void endNomination();

public slots:
  void nominationDone(std::shared_ptr<ICEPair> connection);

protected:
    void run();

    QList<std::shared_ptr<ICEPair>> *candidates_;
    uint32_t sessionID_;

    // temporary storage for succeeded pairs, when both RTP and RTCP
    // of some candidate pair succeeds, endNomination() signal is emitted
    // and the succeeded pair is copied to nominated_rtp_ and nominated_rtcp_
    //
    // the first candidate pair that has both RTP and RTCP tested is chosen
    QMap<QString,
      std::pair<
        std::shared_ptr<ICEPair>,
        std::shared_ptr<ICEPair>
      >
    > nominated_;

    std::shared_ptr<ICEPair> nominated_rtp_;
    std::shared_ptr<ICEPair> nominated_rtcp_;
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
