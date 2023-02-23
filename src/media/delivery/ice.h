#pragma once


#include "../../initiation/negotiation/sdptypes.h"
#include "icetypes.h"

#include <QHostAddress>
#include <QString>
#include <QWaitCondition>

#include <memory>

/* This class represents the ICE protocol component in the flow. The ICE
 * protocol is used to the best pathway for the media by performing connectivity
 * tests. The parameters of these tests are added to the SDP message and once
 * both parties have received the parameters, the tests begin. */

class IceSessionTester;
class StatisticsInterface;

enum ICEState{
  ICE_RUNNING,
  ICE_FINISHED,
  ICE_FAILED
};

class ICE : public QObject
{
  Q_OBJECT

public:
  ICE(uint32_t sessionID, StatisticsInterface* stats);
  ~ICE();

  // Call this function to start the connectivity check/nomination process.
  // The other side should start negotiation as fast as possible
  // Does not block
  void startNomination(const MediaInfo& local, const MediaInfo& remote, bool controller);

  // free all ICE-related resources
  void uninit();

private slots:
  // saves the nominated pair so it can be fetched later on and
  // sends nominationSucceeded signal that negotiation is done
  void handeICESuccess(std::vector<std::shared_ptr<ICEPair>> &streams);

  // ends testing and emits nominationFailed
  void handleICEFailure(std::vector<std::shared_ptr<ICEPair>> &candidates);

signals:

  void mediaNominationSucceeded(uint32_t sessionID, MediaInfo local, MediaInfo remote);
  void mediaNominationFailed(uint32_t sessionID, MediaInfo local, MediaInfo remote);

private:

  // Takes a list of local and remote candidates, matches them based component
  // and returns a list of all possible ICEPairs used for connectivity checks
  std::vector<std::shared_ptr<ICEPair>> makeCandidatePairs(const QList<std::shared_ptr<ICEInfo> > &local,
                                                           const QList<std::shared_ptr<ICEInfo> > &remote,
                                                           bool controller);

  uint64_t pairPriority(int controllerCandidatePriority, int controlleeCandidatePriority) const;

  void printSuccessICEPairs(std::vector<std::shared_ptr<ICEPair> > &streams) const;

  // update MediaInfo of SDP after ICE has finished
  void setMediaPair(MediaInfo& media, std::shared_ptr<ICEInfo> mediaInfo, bool local);

  void updateMedia(MediaInfo& oldMedia, const MediaInfo& newMedia);

  bool isLocalAddress(std::shared_ptr<ICEInfo> info);

  struct MediaNomination
  {
    ICEState state;
    MediaInfo localMedia;
    MediaInfo remoteMedia;
    bool addedToStats;

    std::vector<std::shared_ptr<ICEPair>> candidatePairs;
    std::vector<std::shared_ptr<ICEPair>> succeededPairs;
    std::unique_ptr<IceSessionTester> iceTester;

    int components;
  };

  bool matchNominationList(ICEState state, int& index, const std::vector<MediaNomination>& list,
                           const std::vector<std::shared_ptr<ICEPair>> pairs);

  uint32_t sessionID_;
  std::vector<MediaNomination> mediaNominations_;

  StatisticsInterface* stats_;
};
