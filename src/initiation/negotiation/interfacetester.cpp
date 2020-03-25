#include "interfacetester.h"

#include "connectiontester.h"

InterfaceTester::InterfaceTester()
{}


bool InterfaceTester::bindInterface(QHostAddress interface, quint16 port)
{
  return udp_.bindSocket(interface, port, true);
}


void InterfaceTester::startTestingPairs(bool controller)
{

  for (auto& pair : pairs_)
  {
    workerThreads_.push_back(std::shared_ptr<ConnectionTester>(new ConnectionTester(&udp_, true)));

    QObject::connect(workerThreads_.back().get(),
                     &ConnectionTester::testingDone,
                     this,
                     &InterfaceTester::candidateFound,
                     Qt::DirectConnection);

    // when the UDPServer receives a datagram from remote->address:remote->port,
    // it will send a signal containing the datagram to this Stun object
    //
    // This way multiple Stun instances can listen to same socket
    // TODO: Does not work with STUN candidates
    udp_.expectReplyFrom(workerThreads_.back(),
                         pair->remote->address,
                         pair->remote->port);

    workerThreads_.back()->setCandidatePair(pair);
    workerThreads_.back()->isController(controller);
    workerThreads_.back()->start();
  }
}


void InterfaceTester::endTests()
{
  // kill all threads, regardless of whether nomination succeeded or not
  for (size_t i = 0; i < workerThreads_.size(); ++i)
  {
    workerThreads_[i]->quit();
    workerThreads_[i]->wait();
  }

  udp_.unbind();
}
