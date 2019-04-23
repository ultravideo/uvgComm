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
  pair_ = pair;
}

void ConnectionTester::isController(bool controller)
{
  controller_ = controller;
}

void ConnectionTester::setStun(Stun *stun)
{
  stun_ = stun;

  QObject::connect(this, &ConnectionTester::stopTesting, stun, &Stun::stopTesting);
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
    qDebug() << "Unable to test connection, candidate is NULL!";
    return;
  }

  pair_->state = PAIR_IN_PROGRESS;

  if (!stun_->sendBindingRequest(pair_.get(), controller_))
  {
    qDebug() << "Connectivity checks failed for"
             << pair_->local->address  << pair_->local->port
             << pair_->remote->address << pair_->remote->port;
    return;
  }

  pair_->state = PAIR_SUCCEEDED;

  // controller performs the nomination process in FlowController so exit from ConnectionTester when this connection has been tested...
  if (controller_)
  {
    qDebug() << "pair success" << pair_->local->address << pair_->local->port << pair_->remote->address << pair_->remote->port << pair_->local->component;
    emit testingDone(pair_);
    return;
  }

  //... otherwise start waitin for nomination requests
  if (!stun_->sendNominationResponse(pair_.get()))
  {
    qDebug() << "failed to receive nomination for candidate:\n"
             << "\tlocal:"  << pair_->local->address  << ":" << pair_->local->port << "\n"
             << "\tremote:" << pair_->remote->address << ":" << pair_->remote->port;
    pair_->state = PAIR_FAILED;
    return;
  }

  emit testingDone(pair_);
}
