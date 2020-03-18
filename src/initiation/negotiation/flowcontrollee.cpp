#include "flowcontrollee.h"

#include "connectiontester.h"

#include "common.h"

#include <QEventLoop>

FlowControllee::FlowControllee()
{}


void FlowControllee::run()
{
  QTimer timer;
  QEventLoop loop;

  if (candidates_ == nullptr || candidates_->size() == 0)
  {
    printDebug(DEBUG_ERROR, "FlowAgent",
        "FlowControllee: Invalid candidates, unable to perform ICE candidate negotiation!");
    emit ready(nullptr, nullptr, sessionID_);
    return;
  }

  // because we can only bind to a port once (no multithreaded access), we must divide
  // the candidates into "buckets" and each ConnectionTester is responsible for testing the bucket
  //
  // Each bucket contains all candidates for every interface.
  // ConnectionTester then binds to this interface and takes care of sending the STUN Binding requests
  std::vector<std::unique_ptr<ConnectionTester>> workerThreads;
  QList<ConnectionBucket> buckets;

  int bucketNum     = -1;
  QString prevAddr  = "";
  uint16_t prevPort = 0;

  for (int i = 0; i < candidates_->size(); ++i)
  {
    if (candidates_->at(i)->local->address != prevAddr ||
        candidates_->at(i)->local->port != prevPort)
    {
      bucketNum++;
      buckets.push_back({
          .server = new UDPServer,
          .pairs  = QList<PairStunInstance>()
      });

      // because we cannot modify create new objects from child threads (in this case new socket)
      // we must initialize both UDPServer and Stun objects here so that ConnectionTester doesn't have to do
      // anything but to test the connection
      //
      // Binding might fail, if so happens no STUN objects are created for this socket
      if (buckets[bucketNum].server->bindMultiplexed(
            QHostAddress(candidates_->at(i)->local->address),
            candidates_->at(i)->local->port) == false)
      {
        delete buckets[bucketNum].server;
        buckets[bucketNum].server = nullptr;
      }
    }

    if (buckets[bucketNum].server == nullptr)
    {
      continue;
    }

    buckets[bucketNum].pairs.push_back({
        .stun = new Stun(buckets[bucketNum].server),
        .pair = candidates_->at(i)
    });

    // when the UDPServer receives a datagram from remote->address:remote->port,
    // it will send a signal containing the datagram to this Stun object
    //
    // This way multiple Stun instances can listen to same socket
    buckets[bucketNum].server->expectReplyFrom(buckets[bucketNum].pairs.back().stun,
                                               candidates_->at(i)->remote->address,
                                               candidates_->at(i)->remote->port);

    prevAddr = candidates_->at(i)->local->address;
    prevPort = candidates_->at(i)->local->port;
  }

  for (int i = 0; i < buckets.size(); ++i)
  {
    for (int k = 0; k < buckets[i].pairs.size(); ++k)
    {
      workerThreads.push_back(std::make_unique<ConnectionTester>());

      QObject::connect(
          workerThreads.back().get(),
          &ConnectionTester::testingDone,
          this,
          &FlowAgent::nominationDone,
          Qt::DirectConnection
      );

      buckets[i].pairs[k].stun->moveToThread(workerThreads.back().get());
      workerThreads.back()->setCandidatePair(buckets[i].pairs[k].pair);
      workerThreads.back()->setStun(buckets[i].pairs[k].stun);
      workerThreads.back()->isController(false);
      workerThreads.back()->start();
    }
  }

  bool nominationSucceeded = waitForEndOfNomination(20000);

  // kill all threads, regardless of whether nomination succeeded or not
  for (size_t i = 0; i < workerThreads.size(); ++i)
  {
    workerThreads[i]->quit();
    workerThreads[i]->wait();
  }

  // deallocate all memory consumed by connection buckets
  for (int i = 0; i < buckets.size(); ++i)
  {
    if (buckets[i].server)
    {
      buckets[i].server->unbind();
      delete buckets[i].server;
    }

    for (int k = 0; k < buckets[i].pairs.size(); ++k)
    {
      delete buckets[i].pairs[k].stun;
    }
  }

  // wait for nomination from remote, wait at most 20 seconds
  if (!nominationSucceeded)
  {
    printDebug(DEBUG_ERROR, "FlowAgent",
        "Nomination from remote was not received in time!");
    emit ready(nullptr, nullptr, sessionID_);
    return;
  }

  // media transmission can be started
  emit ready(nominated_rtp_, nominated_rtcp_, sessionID_);
}
