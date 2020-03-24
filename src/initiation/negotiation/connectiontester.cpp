#include "common.h"
#include "connectiontester.h"
#include "stun.h"

ConnectionTester::ConnectionTester():
  pair_(nullptr),
  controller_(false)
{
}

ConnectionTester::~ConnectionTester()
{
}

void ConnectionTester::setCandidatePair(std::shared_ptr<ICEPair> pair)
{
  Q_ASSERT(pair != nullptr);

  pair_ = pair;
}

void ConnectionTester::isController(bool controller)
{
  controller_ = controller;
}

void ConnectionTester::setStun(Stun *stun)
{
  Q_ASSERT(stun != nullptr);

  stun_ = stun;

  QObject::connect(
      this,
      &ConnectionTester::stopTesting,
      stun,
      &Stun::stopTesting,
      Qt::DirectConnection
  );
}

void ConnectionTester::quit()
{
  emit stopTesting();
  QThread::quit();
}

void ConnectionTester::run()
{
  if (pair_ == nullptr)
  {
    printDebug(DEBUG_ERROR, this,
        "Unable to test connection, candidate is NULL!");
    return;
  }

  pair_->state = PAIR_IN_PROGRESS;

  if (!stun_->sendBindingRequest(pair_.get(), controller_))
  {
    printNormal(this,   "No connection found.", {"Path"},
    {pair_->local->address + ":" + QString::number(pair_->local->port) + " -> " +
     pair_->remote->address + ":" + QString::number(pair_->remote->port)});
    return;
  }

  pair_->state = PAIR_SUCCEEDED;

  // controller performs the nomination process in FlowAgent
  // so exit from ConnectionTester when this connection has been tested...
  if (controller_)
  {
    printNormal(this, "Controller testing succeeded", {"Pair"}, {
                  pair_->local->address + ":" + QString::number(pair_->local->port) + " <-> " +
                  pair_->remote->address + ":" + QString::number(pair_->remote->port)});

    emit testingDone(pair_);
    return;
  }

  //... otherwise start waiting for nomination requests
  if (!stun_->sendNominationResponse(pair_.get()))
  {
    printDebug(DEBUG_ERROR, this,  "Failed to receive nomination for candidate: ",
               {"Local", "Remote"},
               {pair_->local->address + ":" + QString::number(pair_->local->port),
                pair_->remote->address + ":" + QString::number(pair_->remote->port)});
    pair_->state = PAIR_FAILED;
    return;
  }

  printNormal(this, "Non-Controller testing succeeded", {"Pair"}, {
                pair_->local->address + ":" + QString::number(pair_->local->port) + " <-> " +
                pair_->remote->address + ":" + QString::number(pair_->remote->port)});

  emit testingDone(pair_);
}
