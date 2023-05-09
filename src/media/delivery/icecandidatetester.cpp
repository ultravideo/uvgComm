#include "icecandidatetester.h"

#include "icepairtester.h"

#include "common.h"
#include "logger.h"

ICECandidateTester::ICECandidateTester()
{}


bool ICECandidateTester::bindInterface(QHostAddress interface, quint16 port)
{
  QObject::connect(&udp_, &UDPServer::datagramAvailable,
                   this, &ICECandidateTester::routeDatagram);

  return udp_.bindSocket(interface, port);
}


void ICECandidateTester::startTestingPairs(bool controller)
{
  if (udp_.isBound())
  {
    for (auto& pair : pairs_)
    {
      workerThreads_.push_back(std::shared_ptr<ICEPairTester>(new ICEPairTester(&udp_)));

      if (controller)
      {
        QObject::connect(workerThreads_.back().get(), &ICEPairTester::controllerPairSucceeded,
                         this,                        &ICECandidateTester::controllerPairFound,
                         Qt::DirectConnection);
      }
      else
      {
        QObject::connect(workerThreads_.back().get(), &ICEPairTester::controlleeNominationDone,
                         this,                        &ICECandidateTester::controlleeNominationDone,
                         Qt::DirectConnection);
      }

      // when the UDPServer receives a datagram from address:port,
      // it will hand the containing the datagram to this tester. This way multiple
      // testers can listen to same socket
      listenerMutex_.lock();
      listeners_[pair->remote->address][pair->remote->port] = workerThreads_.back();
      listenerMutex_.unlock();

      workerThreads_.back()->setCandidatePair(pair);
      workerThreads_.back()->setController(controller);

      // starts the binding tests. For non-controller also handles the nomination.
      workerThreads_.back()->start();
    }
  }
  else
  {
    Logger::getLogger()->printWarning(this, "Tried to start testing, "
                                            "but the socket was not bound.");
  }
}


void ICECandidateTester::endTests()
{
  if (udp_.isBound())
  {
    // kill all threads, regardless of whether nomination succeeded or not
    for (size_t i = 0; i < workerThreads_.size(); ++i)
    {
      workerThreads_[i]->quit();
      workerThreads_[i]->wait();
    }
    workerThreads_.clear();
    udp_.unbind();
    listenerMutex_.lock();
    listeners_.clear();
    listenerMutex_.unlock();
  }
}


bool ICECandidateTester::performNomination(std::vector<std::shared_ptr<ICEPair>>& nominated)
{
  workerThreads_.clear();
  workerThreads_.push_back(std::shared_ptr<ICEPairTester>(new ICEPairTester(&udp_)));

  connect(&udp_,                       &UDPServer::datagramAvailable,
          workerThreads_.back().get(), &ICEPairTester::recvStunMessage);

  for (auto& pair : nominated)
  {
    if (!udp_.bindSocket(workerThreads_.back()->getLocalAddress(pair->local),
                         workerThreads_.back()->getLocalPort(pair->local)))
    {
      return false;
    }
    if (!workerThreads_.back()->sendNominationWaitResponse(pair.get()))
    {
      udp_.unbind();
      Logger::getLogger()->printError(this,  "Failed to nominate a pair in for candidate!");
      return false;
    }

    udp_.unbind();
    pair->state  = PAIR_NOMINATED;
  }

  return true;
}


void ICECandidateTester::routeDatagram(QNetworkDatagram message)
{
  listenerMutex_.lock();
  // is anyone listening to messages from this sender?
  if (listeners_.contains(message.senderAddress().toString()) &&
      listeners_[message.senderAddress().toString()].contains(message.senderPort()))
  {
    std::shared_ptr<ICEPairTester> listener
        = listeners_[message.senderAddress().toString()][message.senderPort()];

    listenerMutex_.unlock();
    listener->recvStunMessage(message);
  }
  else if (!message.senderAddress().isNull() && message.senderPort() != -1)
  {
    listenerMutex_.unlock();
    // TODO: This is where we should detect if we should add Peer Reflexive candidates.
    Logger::getLogger()->printWarning(this, "We have encountered a peer reflexive candidate "
                                            "which has not been implemented!",
                                            "Address", message.senderAddress().toString() + ":" +
                                      QString::number(message.senderPort()));
  }
  else
  {
    listenerMutex_.unlock();
  }
}
