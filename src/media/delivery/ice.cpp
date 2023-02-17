#include "ice.h"

#include "icesessiontester.h"
#include "logger.h"
#include "common.h"

#include <QNetworkInterface>
#include <QSettings>

#include <memory>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <math.h>       /* pow */

ICE::ICE(uint32_t sessionID):
  sessionID_(sessionID),
  mediaNominations_()
{
  qRegisterMetaType<uint32_t>("uint32_t");
  qRegisterMetaType<MediaInfo>("MediaInfo");
}


ICE::~ICE()
{
  uninit();
}


void ICE::startNomination(const MediaInfo &local, const MediaInfo &remote, bool controller)
{
  if (controller)
  {
    Logger::getLogger()->printImportant(this, "Starting ICE Media nomination as controller");
  }
  else
  {
    Logger::getLogger()->printImportant(this, "Starting ICE Media nomination as controllee");
  }

  std::vector<std::shared_ptr<ICEPair>> newCandidates = makeCandidatePairs(local.candidates,
                                                                           remote.candidates, controller);

  int matchIndex = 0;
  if (matchNominationList(ICE_FINISHED, matchIndex, mediaNominations_, newCandidates))
  {
    Logger::getLogger()->printNormal(this, "Found existing ICE results, using those");

    updateMedia(mediaNominations_[matchIndex].localMedia, local);
    updateMedia(mediaNominations_[matchIndex].remoteMedia, remote);
    emit mediaNominationSucceeded(sessionID_,
                                  mediaNominations_[matchIndex].localMedia,
                                  mediaNominations_[matchIndex].remoteMedia);
  }
  else if (matchNominationList(ICE_RUNNING, matchIndex, mediaNominations_, newCandidates))
  {
    Logger::getLogger()->printNormal(this, "Already running ICE with these candidates, not doing anything");
  }
  else if (matchNominationList(ICE_FAILED, matchIndex, mediaNominations_, newCandidates))
  {
    Logger::getLogger()->printError(this, "These ICE candidates have failed before, no sense in running them again");
  }
  else
  {
    int components = 1;

    if (local.proto == "RTP/AVP" ||
        local.proto == "RTP/AVPF" ||
        local.proto == "RTP/SAVP" ||
        local.proto == "RTP/SAVPF")
    {
      components = 2; // RTP + RTCP
    }

    mediaNominations_.push_back({ICE_RUNNING, local, remote,
                                 newCandidates, {},
                                 std::unique_ptr<IceSessionTester> (new IceSessionTester(controller)),
                                 components});

    Logger::getLogger()->printNormal(this, "No previous matching ICE results, performing nomination");

    // perform connection testing and use those instead
    QObject::connect(mediaNominations_.back().iceTester.get(),
                     &IceSessionTester::iceSuccess,
                     this,
                     &ICE::handeICESuccess,
                     Qt::DirectConnection);
    QObject::connect(mediaNominations_.back().iceTester.get(),
                     &IceSessionTester::iceFailure,
                     this,
                     &ICE::handleICEFailure,
                     Qt::DirectConnection);

    /* Starts a SessionTester which is responsible for handling connectivity checks and nomination.
     * When testing is finished it is connected tonominationSucceeded/nominationFailed */
    mediaNominations_.back().iceTester.get()->init(&mediaNominations_.back().candidatePairs, components);
    mediaNominations_.back().iceTester.get()->start();
  }
}


bool ICE:: matchNominationList(ICEState state, int& index, const std::vector<MediaNomination> &list,
                               const std::vector<std::shared_ptr<ICEPair> > pairs)
{
  for (int i = 0; i < list.size(); ++i)
  {
    if (sameCandidates(pairs, list[i].candidatePairs))
    {
      index = i;
      return true;
    }
  }

  index = -1;
  return false;
}


void ICE::handeICESuccess(std::vector<std::shared_ptr<ICEPair> > &streams)
{
  // find the media these streams belong to
  for (auto& media : mediaNominations_)
  {
    // change state of media nomination and emit signal for ICE completion
    if (containCandidates(streams, media.candidatePairs))
    {
      Logger::getLogger()->printNormal(this, "ICE succeeded", "Components",
                                       QString::number(streams.size()));
      media.state = ICE_FINISHED;
      media.succeededPairs = media.candidatePairs;
      media.candidatePairs.clear();
      media.iceTester->quit();

      printSuccessICEPairs(streams);

      // TODO: Improve this (probably need to add RTCP to SDP message)

      // 0 is RTP, 1 is RTCP
      if (streams.at(0) != nullptr && streams.at(1) != nullptr)
      {
        setMediaPair(media.localMedia,  streams.at(0)->local, true);
        setMediaPair(media.remoteMedia, streams.at(0)->remote, false);
      }

      emit mediaNominationSucceeded(sessionID_, media.localMedia, media.remoteMedia);

      return;
    }
  }

  Logger::getLogger()->printProgramError(this, "Did not find the media the successful ICE belongs to");
}


void ICE::handleICEFailure(std::vector<std::shared_ptr<ICEPair> > *candidates)
{
  Logger::getLogger()->printDebug(DEBUG_ERROR, "ICE",
                                  "Failed to nominate RTP/RTCP candidates!");

  for (auto& media : mediaNominations_)
  {
    // change state of media nomination and emit signal for ICE completion
    if (sameCandidates(*candidates, media.candidatePairs))
    {
      media.state = ICE_FAILED;
      media.iceTester->quit();

      emit mediaNominationFailed(sessionID_, media.localMedia, media.remoteMedia);
    }
  }

  Logger::getLogger()->printProgramError(this, "Did not find the media ICE failure belongs to");
}


bool ICE::containCandidates(std::vector<std::shared_ptr<ICEPair>> &streams,
                            std::vector<std::shared_ptr<ICEPair>> allCandidates)
{
  int matchingStreams = 0;
  for (auto& candidatePair : allCandidates)
  {
    for (auto& readyConnection : streams)
    {
      if (sameCandidate(readyConnection->local, candidatePair->local) &&
          sameCandidate(readyConnection->remote, candidatePair->remote))
      {
        ++matchingStreams;
      }
    }
  }

  return streams.size() == matchingStreams;
}

void ICE::printSuccessICEPairs(std::vector<std::shared_ptr<ICEPair> > &streams) const
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
}


std::vector<std::shared_ptr<ICEPair> > ICE::makeCandidatePairs(
    const QList<std::shared_ptr<ICEInfo>>& local,
    const QList<std::shared_ptr<ICEInfo>>& remote,
    bool controller
)
{
  std::vector<std::shared_ptr<ICEPair>> pairs;

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


void ICE::uninit()
{
  for (auto& media : mediaNominations_)
  {
    if (media.state == ICE_RUNNING)
    {
      media.iceTester->exit(0);
      uint8_t waits = 0;
      while (media.iceTester->isRunning() && waits <= 50)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ++waits;
      }
    }
  }

  mediaNominations_.clear();
}


int ICE::pairPriority(int controllerCandidatePriority, int controlleeCandidatePriority) const
{
  // see RFC 8445 section 6.1.2.3
  return ((int)pow(2, 32) * qMin(controllerCandidatePriority, controlleeCandidatePriority)) +
         ((int)2 * qMax(controllerCandidatePriority, controlleeCandidatePriority)) +
         controllerCandidatePriority > controlleeCandidatePriority ? 1 : 0;
}


bool ICE::sameCandidates(std::vector<std::shared_ptr<ICEPair> > newCandidates,
                         std::vector<std::shared_ptr<ICEPair> > oldCandidates)
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


void ICE::setMediaPair(MediaInfo& media, std::shared_ptr<ICEInfo> mediaInfo, bool local)
{
  if (mediaInfo == nullptr)
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, "SDPNegotiationHelper",
                                    "Null mediainfo in setMediaPair");
    return;
  }

  // for local address, we bind to our rel-address if using non-host connection type
  if (local &&
      mediaInfo->type != "host" &&
      mediaInfo->rel_address != "" && mediaInfo->rel_port != 0)
  {
    setSDPAddress(mediaInfo->rel_address, media.connection_address,
                  media.connection_nettype,
                  media.connection_addrtype);
    media.receivePort        = mediaInfo->rel_port;
  }
  else
  {
    setSDPAddress(mediaInfo->address, media.connection_address,
                  media.connection_nettype,
                  media.connection_addrtype);
    media.receivePort        = mediaInfo->port;
  }
}


void ICE::updateMedia(MediaInfo& oldMedia, const MediaInfo &newMedia)
{
  // basically update everything expect ICE connection
  oldMedia.type = newMedia.type;
  oldMedia.proto = newMedia.proto;

  oldMedia.rtpNums = newMedia.rtpNums;
  oldMedia.title = newMedia.title;
  oldMedia.bitrate = newMedia.bitrate;
  oldMedia.encryptionKey = newMedia.encryptionKey;

  oldMedia.codecs = newMedia.codecs;
  oldMedia.flagAttributes = newMedia.flagAttributes;
  oldMedia.valueAttributes = newMedia.valueAttributes;
  oldMedia.candidates = newMedia.candidates;
}
