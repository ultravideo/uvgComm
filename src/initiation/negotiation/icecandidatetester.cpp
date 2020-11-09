#include "icecandidatetester.h"

#include "icepairtester.h"

#include "common.h"

IceCandidateTester::IceCandidateTester()
{}


bool IceCandidateTester::bindInterface(QHostAddress interface, quint16 port)
{
  QObject::connect(&udp_, &UDPServer::datagramAvailable,
                   this, &IceCandidateTester::routeDatagram);

  return udp_.bindSocket(interface, port);
}


void IceCandidateTester::startTestingPairs(bool controller)
{
  if (udp_.isBound())
  {
    for (auto& pair : pairs_)
    {
      workerThreads_.push_back(std::shared_ptr<IcePairTester>(new IcePairTester(&udp_)));

      if (controller)
      {
        QObject::connect(workerThreads_.back().get(), &IcePairTester::controllerPairSucceeded,
                         this,                        &IceCandidateTester::controllerPairFound,
                         Qt::DirectConnection);
      }
      else
      {
        QObject::connect(workerThreads_.back().get(), &IcePairTester::controlleeNominationDone,
                         this,                        &IceCandidateTester::controlleeNominationDone,
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
    printWarning(this, "Tried to start testing, but the socket was not bound.");
  }
}


void IceCandidateTester::endTests()
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


bool IceCandidateTester::performNomination(QList<std::shared_ptr<ICEPair>>& nominated)
{
  workerThreads_.clear();
  workerThreads_.push_back(std::shared_ptr<IcePairTester>(new IcePairTester(&udp_)));

  connect(&udp_,                       &UDPServer::datagramAvailable,
          workerThreads_.back().get(), &IcePairTester::recvStunMessage);

  for (auto& pair : nominated)
  {
    if (!udp_.bindSocket(workerThreads_.back()->getLocalAddress(pair->local),
                         workerThreads_.back()->getLocalPort(pair->local)))
    {
      return false;
    }
    if (!workerThreads_.back()->sendNominationRequest(pair.get()))
    {
      udp_.unbind();
      printError(this,  "Failed to nominate a pair in for candidate!");
      return false;
    }

    udp_.unbind();
    pair->state  = PAIR_NOMINATED;
  }

  return true;
}


void IceCandidateTester::routeDatagram(QNetworkDatagram message)
{
  listenerMutex_.lock();
  // is anyone listening to messages from this sender?
  if (listeners_.contains(message.senderAddress().toString()) &&
      listeners_[message.senderAddress().toString()].contains(message.senderPort()))
  {
    std::shared_ptr<IcePairTester> listener
        = listeners_[message.senderAddress().toString()][message.senderPort()];

    listenerMutex_.unlock();
    listener->recvStunMessage(message);
  }
  else if (!message.senderAddress().isNull() && message.senderPort() != -1)
  {
    listenerMutex_.unlock();
    // TODO: This is where we should detect if we should add Peer Reflexive candidates.

    printWarning(this, "Found a peer reflexive candidate. Not implemented.", {"Address"}, {
                 message.destinationAddress().toString() + ":" +
                 QString::number(message.destinationPort()) + " <- " +
                 message.senderAddress().toString() + ":" +
                 QString::number(message.senderPort())});
  }
  else
  {
    printWarning(this, "Message ", {"The sender address or port were not set in network package"}, {
                 message.destinationAddress().toString() + ":" +
                 QString::number(message.destinationPort()) + " <- " +
                 message.senderAddress().toString() + ":" +
                 QString::number(message.senderPort())});
    listenerMutex_.unlock();
  }
}
