#include <QEventLoop>
#include <QTimer>
#include <QThread>

#include "common.h"
#include "connectiontester.h"
#include "flowagent.h"
#include "ice.h"


/* the concept of ConnectionBucket is explained elsewhere */
struct ConnectionBucket
{
  UDPServer *server;
  QList<std::shared_ptr<ICEPair>> pairs;
};


FlowAgent::FlowAgent(bool controller, int timeout):
  candidates_(nullptr),
  sessionID_(0),
  controller_(controller),
  timeout_(timeout)
{
}

FlowAgent::~FlowAgent()
{
}

void FlowAgent::setCandidates(QList<std::shared_ptr<ICEPair>> *candidates)
{
  Q_ASSERT(candidates != nullptr);

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


void FlowAgent::run()
{
  if (candidates_ == nullptr || candidates_->size() == 0)
  {
    printDebug(DEBUG_ERROR, this,
               "Invalid candidates, unable to perform ICE candidate negotiation!");
    emit ready(nullptr, nullptr, sessionID_);
    return;
  }

  // because we can only bind to a port once (no multithreaded access), we must divide
  // the candidates into "buckets" and each ConnectionTester is responsible for testing the bucket
  //
  // Each bucket contains all candidates for one interface.
  // ConnectionTester then binds to this interface and takes care of sending the STUN Binding requests
  std::vector<std::shared_ptr<ConnectionTester>> workerThreads;
  QList<ConnectionBucket> buckets;

  QString prevAddr  = "";
  uint16_t prevPort = 0;

  for (int i = 0; i < candidates_->size(); ++i)
  {
    if (candidates_->at(i)->local->address != prevAddr ||
        candidates_->at(i)->local->port != prevPort)
    {
      buckets.push_back({new UDPServer, QList<std::shared_ptr<ICEPair>>()});

      // because we cannot modify create new objects from child threads (in this case new socket)
      // we must initialize both UDPServer and Stun objects here so that ConnectionTester doesn't have to do
      // anything but to test the connection
      //
      // Binding might fail, if so happens no STUN objects are created for this socket
      if (!buckets.back().server->bindSocket(
            QHostAddress(candidates_->at(i)->local->address),
            candidates_->at(i)->local->port, true))
      {
        delete buckets.back().server;
        buckets.back().server = nullptr;
        continue;
      }
    }

    buckets.back().pairs.push_back({candidates_->at(i)});

    // this keeps the code from crashing using magic. Do not move.
    prevAddr = candidates_->at(i)->local->address;
    prevPort = candidates_->at(i)->local->port;
  }

  for (int i = 0; i < buckets.size(); ++i)
  {
    for (int j = 0; j < buckets[i].pairs.size(); ++j)
    {
      workerThreads.push_back(std::shared_ptr<ConnectionTester>(new ConnectionTester(buckets[i].server, true)));

      // when the UDPServer receives a datagram from remote->address:remote->port,
      // it will send a signal containing the datagram to this Stun object
      //
      // This way multiple Stun instances can listen to same socket
      buckets[i].server->expectReplyFrom(workerThreads.back(),
                                         buckets[i].pairs.at(j)->remote->address,
                                         buckets[i].pairs.at(j)->remote->port);

      QObject::connect(
          workerThreads.back().get(),
          &ConnectionTester::testingDone,
          this,
          &FlowAgent::nominationDone,
          Qt::DirectConnection
      );

      workerThreads.back()->setCandidatePair(buckets[i].pairs[j]);
      workerThreads.back()->isController(controller_);
      workerThreads.back()->start();
    }
  }

  bool nominationSucceeded = waitForEndOfNomination(timeout_);

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
  }

  // wait for nomination from remote, wait at most 20 seconds
  if (!nominationSucceeded)
  {
    printError(this, "Nomination from remote was not received in time!");
    emit ready(nullptr, nullptr, sessionID_);
    return;
  }

  if (controller_)
  {
    ConnectionTester tester(new UDPServer, false);

    if (!tester.sendNominationRequest(nominated_rtp_.get()))
    {
      printDebug(DEBUG_ERROR, "FlowAgent",  "Failed to nominate RTP candidate!");
      emit ready(nullptr, nullptr, sessionID_);
      return;
    }

    if (!tester.sendNominationRequest(nominated_rtcp_.get()))
    {
      printDebug(DEBUG_ERROR, "FlowAgent",  "Failed to nominate RTCP candidate!");
      emit ready(nominated_rtp_, nullptr, sessionID_);
      return;
    }

    nominated_rtp_->state  = PAIR_NOMINATED;
    nominated_rtcp_->state = PAIR_NOMINATED;
  }

  printImportant(this, "Nomination finished", {"Winning pair"}, {
                   nominated_rtp_->local->address  + ":" + QString::number(nominated_rtp_->local->port) + " <-> " +
                   nominated_rtp_->remote->address + ":" + QString::number(nominated_rtp_->remote->port)});

  emit ready(nominated_rtp_, nominated_rtcp_, sessionID_);
}
