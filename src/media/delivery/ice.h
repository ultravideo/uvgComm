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

class ICE : public QObject
{
  Q_OBJECT

  public:
  ICE(uint32_t sessionID);
  ~ICE();

  // Call this function to start the connectivity check/nomination process.
  // The other side should start negotiation as fast as possible
  // Does not block
  void startNomination(int components, QList<std::shared_ptr<ICEInfo>>& local,
                       QList<std::shared_ptr<ICEInfo>>& remote, bool controller);

  // free all ICE-related resources
  void uninit();

private slots:
  // saves the nominated pair so it can be fetched later on and
  // sends nominationSucceeded signal that negotiation is done
  void handeICESuccess(QList<std::shared_ptr<ICEPair>> &streams);

  // ends testing and emits nominationFailed
  void handleICEFailure();

signals:
  void nominationFailed(quint32 sessionID);
  void nominationSucceeded(QList<std::shared_ptr<ICEPair>>& streams,
                           quint32 sessionID);
private:

  // Takes a list of local and remote candidates, matches them based component
  // and returns a list of all possible ICEPairs used for connectivity checks
  QList<std::shared_ptr<ICEPair>> makeCandidatePairs(QList<std::shared_ptr<ICEInfo>>& local,
                                                     QList<std::shared_ptr<ICEInfo>>& remote, bool controller);

  int pairPriority(int controllerCandidatePriority, int controlleeCandidatePriority) const;

  uint32_t sessionID_;

  std::shared_ptr<IceSessionTester> agent_;

  // list of all candidates, remote and local
  QList<std::shared_ptr<ICEPair>> pairs_;

  bool connectionNominated_;

  int components_;

  SDPMessageInfo localSDP_;
  SDPMessageInfo remoteSDP_;
};
