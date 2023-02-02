#include "ice.h"

#include "icesessiontester.h"
#include "logger.h"

#include <QNetworkInterface>
#include <QSettings>

#include <memory>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <math.h>       /* pow */

ICE::ICE(uint32_t sessionID):
  sessionID_(sessionID),
  agent_(nullptr),
  candidatePairs_(),
  succeededPairs_(),
  connectionNominated_(false),
  components_(0),
  localSDP_(),
  remoteSDP_()
{
  qRegisterMetaType<QVector<int> >("QList<std::shared_ptr<ICEPair> >&");
}


ICE::~ICE()
{
  uninit();
}


void ICE::startNomination(int components, QList<std::shared_ptr<ICEInfo>>& local,
                          QList<std::shared_ptr<ICEInfo>>& remote, bool controller)
{
  Logger::getLogger()->printImportant(this, "Starting ICE nomination");

  components_ = components;

  // wait for previous nomination to finish so we don't delete a running thread
  if (agent_ != nullptr)
  {
    uninit();
  }

  agent_ = std::unique_ptr<IceSessionTester> (new IceSessionTester(controller));
  QList<std::shared_ptr<ICEPair>> newCandidates = makeCandidatePairs(local, remote, controller);

  // see if we our old results are good enough
  if (!succeededPairs_.empty() && sameCandidates(newCandidates, candidatePairs_))
  {
    // use old results
    Logger::getLogger()->printNormal(this, "Found existing ICE results, using those");
    emit nominationSucceeded(succeededPairs_, sessionID_);
  }
  else
  {
    Logger::getLogger()->printNormal(this, "No previous mathinc ICE results, performing nomination");
    // perform connection testing and use those instead
    succeededPairs_.clear();
    candidatePairs_ = newCandidates;

    connectionNominated_ = false;

    QObject::connect(agent_.get(),
                     &IceSessionTester::iceSuccess,
                     this,
                     &ICE::handeICESuccess,
                     Qt::DirectConnection);
    QObject::connect(agent_.get(),
                     &IceSessionTester::iceFailure,
                     this,
                     &ICE::handleICEFailure,
                     Qt::DirectConnection);

    /* Starts a SessionTester which is responsible for handling connectivity checks and nomination.
     * When testing is finished it is connected tonominationSucceeded/nominationFailed */
    agent_->init(&candidatePairs_, components_);
    agent_->start();
  }
}


void ICE::handeICESuccess(QList<std::shared_ptr<ICEPair>> &streams)
{
  // check that results make sense. They should always.
  if (streams.at(0) == nullptr ||
      streams.at(1) == nullptr ||
      streams.size() != components_)
  {
    Logger::getLogger()->printProgramError(this,  "The ICE results don't make " 
                                                  "sense even though they should");
    handleICEFailure();
  }
  else 
  {
    QStringList names;
    QStringList values;
    for(auto& component : streams)
    {
      names.append("Component " + QString::number(component->local->component));
      values.append(component->local->address + ":" + QString::number(component->local->port)
                    + " <-> " +
                    component->remote->address + ":" + QString::number(component->remote->port));
    }

    Logger::getLogger()->printDebug(DEBUG_IMPORTANT, this, "ICE succeeded", names, values);

    // end other tests. We have a winner.
    agent_->quit();
    connectionNominated_ = true;
    succeededPairs_ += streams;

    emit nominationSucceeded(succeededPairs_, sessionID_);
  }
}


QList<std::shared_ptr<ICEPair>> ICE::makeCandidatePairs(
    QList<std::shared_ptr<ICEInfo>>& local,
    QList<std::shared_ptr<ICEInfo>>& remote,
    bool controller
)
{
  QList<std::shared_ptr<ICEPair>> pairs;

  // TODO: Check if local are actually local interfaces

  // match all host candidates with remote (remote does the same)
  for (int i = 0; i < local.size(); ++i)
  {
    for (int k = 0; k < remote.size(); ++k)
    {
      // component has to match
      if (local[i]->component == remote[k]->component)
      {
        std::shared_ptr<ICEPair> pair = std::make_shared<ICEPair>();

        // we copy local because we modify it later with stun bindings and
        // we don't want to modify our sent candidates
        pair->local = std::shared_ptr<ICEInfo> (new ICEInfo);
        *(pair->local)    = *local[i];

        pair->remote   = remote[k];

        if (controller)
        {
          pair->priority = pairPriority(local[i]->priority, remote[k]->priority);
        }
        else
        {
          pair->priority = pairPriority(remote[k]->priority, local[i]->priority);
        }

        pair->state    = PAIR_FROZEN;

        pairs.push_back(pair);
      }
    }
  }

  Logger::getLogger()->printNormal(this, "Created " + QString::number(pairs.size()) +
                                         " candidate pairs");
  return pairs;
}


void ICE::handleICEFailure()
{
  Logger::getLogger()->printDebug(DEBUG_ERROR, "ICE",  
                                  "Failed to nominate RTP/RTCP candidates!");

  agent_->quit();
  connectionNominated_ = false; // TODO: crash here with debugger
  emit nominationFailed(sessionID_);
}


void ICE::uninit()
{
  if (agent_ != nullptr)
  {
    agent_->exit(0);
    uint8_t waits = 0;
    while (agent_->isRunning() && waits <= 10)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      ++waits;
    }
  }

  agent_ = nullptr;
  connectionNominated_ = false;
}


int ICE::pairPriority(int controllerCandidatePriority, int controlleeCandidatePriority) const
{
  // see RFC 8445 section 6.1.2.3
  return ((int)pow(2, 32) * qMin(controllerCandidatePriority, controlleeCandidatePriority)) +
         ((int)2 * qMax(controllerCandidatePriority, controlleeCandidatePriority)) +
         controllerCandidatePriority > controlleeCandidatePriority ? 1 : 0;
}


bool ICE::sameCandidates(QList<std::shared_ptr<ICEPair>> newCandidates,
                         QList<std::shared_ptr<ICEPair>> oldCandidates)
{
  if (newCandidates.empty() || oldCandidates.empty())
  {
    return false;
  }

  for (auto& newCandidate: newCandidates)
  {
    bool candidatesFound = false;

    for (auto& oldCandidate: oldCandidates)
    {
      if (sameCandidate(newCandidate->local, oldCandidate->local) &&
          sameCandidate(newCandidate->remote, oldCandidate->remote))
      {
        // we have a match!
        candidatesFound = true;
      }
    }

    if(!candidatesFound)
    {
      // could not find a match for this candidate, which means we have to perform ICE
      return false;
    }
  }

  return true;
}


bool ICE::sameCandidate(std::shared_ptr<ICEInfo> firstCandidate,
                        std::shared_ptr<ICEInfo> secondCandidate)
{
  return firstCandidate->foundation == secondCandidate->foundation &&
      firstCandidate->component == secondCandidate->component &&
      firstCandidate->transport == secondCandidate->transport &&
      firstCandidate->address == secondCandidate->address &&
      firstCandidate->port == secondCandidate->port &&
      firstCandidate->type == secondCandidate->type &&
      firstCandidate->rel_address == secondCandidate->rel_address &&
      firstCandidate->rel_port == secondCandidate->rel_port;
}
