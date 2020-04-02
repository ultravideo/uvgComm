#pragma once

#include "icetypes.h"

#include <QWaitCondition>
#include <QMutex>
#include <QThread>
#include <QList>
#include <QMap>

#include <memory>

class IceCandidateTester;

class IceSessionTester : public QThread
{
  Q_OBJECT

public:
  IceSessionTester(bool controller, int timeout);
  ~IceSessionTester();

  void init(QList<std::shared_ptr<ICEPair>> *pairs_,
            uint32_t sessionID, uint8_t components);

signals:
  // When IceSessionTester finishes, it sends a ready signal to main thread (ICE).
  // If the nomination succeeded, both candidateRTP and candidateRTCP are valid pointers,
  // otherwise nullptr.
  void iceSuccess(QList<std::shared_ptr<ICEPair>>& streams,
                  uint32_t sessionID);

  void iceFailure(uint32_t sessionID);

  // when both RTP and RTCP of any address succeeds, sends endNomination() signal to FlowAgent (sent by ConnectionTester)
  // so it knows to stop all other ConnectionTester and return the succeeded pair to ICE
  //
  // FlowAgent listens to this signal in waitForEndOfNomination() and if the signal is not received within some time period,
  // FlowAgent fails and returns nullptrs to ICE indicating that the call cannot start
  void endTesting();

public slots:
  void endConcurrentTesting(std::shared_ptr<ICEPair> connection);

protected:
  virtual void run();

private:

  std::shared_ptr<IceCandidateTester> createCandidateTester(std::shared_ptr<ICEInfo> local);

  // wait for endNomination() signal and return true if it's received (meaning the nomination succeeded)
  // if the endNomination() is not received in time the function returns false
  void waitForEndOfTesting(unsigned long timeout);

  QList<std::shared_ptr<ICEPair>> *pairs_;

  uint32_t sessionID_;

  bool controller_;
  int timeout_;

  uint8_t components_;

  // temporary storage for succeeded components, when all components of
  // one candidate pair succeeds, endNomination() signal is emitted
  // and the succeeded pair is copied to nominated_
  //
  // currently the first pair to have all its components succeed is selected.

  QMutex nominated_mtx;
  QMap<QString, QMap<uint8_t, std::shared_ptr<ICEPair>>> finished_;

  QList<std::shared_ptr<ICEPair>> nominated_;
};
