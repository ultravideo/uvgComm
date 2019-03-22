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

signals:
  void ready(ICEPair *candidateRTP, ICEPair *candidateRTCP, uint32_t sessionID);

protected:
    void run();

    QList<ICEPair *> *candidates_;
    uint32_t sessionID_;
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
