#include <QEventLoop>
#include <QTimer>
#include <QThread>

#include "common.h"
#include "connectiontester.h"
#include "flowagent.h"
#include "ice.h"


FlowAgent::FlowAgent():
  candidates_(nullptr),
  sessionID_(0)
{
}

FlowAgent::~FlowAgent()
{
}

void FlowAgent::setCandidates(QList<std::shared_ptr<ICEPair>> *candidates)
{
  Q_ASSERT(candidates != nullptr);
  Q_ASSERT(candidates->size() != 0);

  candidates_ = candidates;
}

void FlowAgent::setSessionID(uint32_t sessionID)
{
  Q_ASSERT(sessionID != 0);

  sessionID_ = sessionID;
}

void FlowAgent::nominationDone(std::shared_ptr<ICEPair> connection)
{
  Q_ASSERT(connection != nullptr);

  nominated_mtx.lock();

  if (connection->local->component == RTP)
  {
    nominated_[connection->local->address].first = connection;
  }
  else
  {
    nominated_[connection->local->address].second = connection;
  }

  if (nominated_[connection->local->address].first  != nullptr &&
      nominated_[connection->local->address].second != nullptr)
  {
    nominated_rtp_  = nominated_[connection->local->address].first;
    nominated_rtcp_ = nominated_[connection->local->address].second;

    nominated_rtp_->state  = PAIR_NOMINATED;
    nominated_rtcp_->state = PAIR_NOMINATED;

    emit endNomination();
    nominated_mtx.unlock();
    return;
  }

  nominated_mtx.unlock();
}

bool FlowAgent::waitForEndOfNomination(unsigned long timeout)
{
  QTimer timer;
  QEventLoop loop;

  timer.setSingleShot(true);

  QObject::connect(
      this,
      &FlowAgent::endNomination,
      &loop,
      &QEventLoop::quit,
      Qt::DirectConnection
  );

  QObject::connect(
      &timer,
      &QTimer::timeout,
      &loop,
      &QEventLoop::quit,
      Qt::DirectConnection
  );

  timer.start(timeout);
  loop.exec();

  return timer.isActive();
}
