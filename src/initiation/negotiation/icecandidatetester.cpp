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
  for (auto& pair : pairs_)
  {
    workerThreads_.push_back(std::shared_ptr<IcePairTester>(new IcePairTester(&udp_, true)));

    QObject::connect(workerThreads_.back().get(),
                     &IcePairTester::testingDone,
                     this,
                     &IceCandidateTester::candidateFound,
                     Qt::DirectConnection);

    // when the UDPServer receives a datagram from remote->address:remote->port,
    // it will send a signal containing the datagram to this Stun object
    //
    // This way multiple Stun instances can listen to same socket
    // TODO: Does not work with STUN candidates
    expectReplyFrom(workerThreads_.back(),
                    pair->remote->address,
                    pair->remote->port);

    workerThreads_.back()->setCandidatePair(pair);
    workerThreads_.back()->isController(controller);
    workerThreads_.back()->start();
  }
}


void IceCandidateTester::endTests()
{
  // kill all threads, regardless of whether nomination succeeded or not
  for (size_t i = 0; i < workerThreads_.size(); ++i)
  {
    workerThreads_[i]->quit();
    workerThreads_[i]->wait();
  }

  workerThreads_.clear();

  udp_.unbind();
}


bool IceCandidateTester::performNomination(std::shared_ptr<ICEPair> rtp,
                                        std::shared_ptr<ICEPair> rtcp)
{
  IcePairTester tester(new UDPServer, false);

  if (!tester.sendNominationRequest(rtp.get()))
  {
    printError(this,  "Failed to nominate RTP candidate!");
    return false;
  }

  if (!tester.sendNominationRequest(rtcp.get()))
  {
    printError(this,  "Failed to nominate RTCP candidate!");
    return false;
  }

  rtp->state  = PAIR_NOMINATED;
  rtcp->state = PAIR_NOMINATED;

  return true;
}


void IceCandidateTester::routeDatagram(QNetworkDatagram message)
{
  // is anyone listening to messages from this sender?
  if (listeners_.contains(message.senderAddress().toString()) &&
      listeners_[message.senderAddress().toString()].contains(message.senderPort()))
  {
    QMetaObject::invokeMethod(
        listeners_[message.senderAddress().toString()][message.senderPort()].get(),
        "recvStunMessage",
        Qt::DirectConnection,
        Q_ARG(QNetworkDatagram, message)
    );
  }
  else
  {
    printError(this, "Could not find listener for data", {"Address"}, {
                 message.destinationAddress().toString() + ":" +
                 QString::number(message.destinationPort()) + " <- " +
                 message.senderAddress().toString() + ":" +
                 QString::number(message.senderPort())});
  }
}


void IceCandidateTester::expectReplyFrom(std::shared_ptr<IcePairTester> ct,
                                      QString& address, quint16 port)
{
    listeners_[address][port] = ct;
}


