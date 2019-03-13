#pragma once

#include <QWaitCondition>
#include <QMutex>
#include <QThread>
#include <QList>

#include "stun.h"

typedef struct ICEPair ICEPair_t;

class FlowAgent : public QThread
{
  Q_OBJECT

public:
    FlowAgent();
    ~FlowAgent();
    void setCandidates(QList<struct ICEPair *> *candidates);

signals:
  void ready(struct ICEPair *candidateRTP, struct ICEPair *candidateRTCP);

protected:
    void run();

protected:
    QList<ICEPair_t *> *candidates_;
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
