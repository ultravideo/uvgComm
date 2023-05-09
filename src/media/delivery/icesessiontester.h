#pragma once

#include "icetypes.h"

#include <QWaitCondition>
#include <QMutex>
#include <QThread>
#include <QList>
#include <QMap>

#include <memory>

class ICECandidateTester;

/* Handles ICE testing for one session. */

// TODO: Make it possible to test multiple session concurrently
// by moving stuff from ICE class to this class

class IceSessionTester : public QThread
{
  Q_OBJECT

public:
  IceSessionTester(bool controller);
  ~IceSessionTester();

  void init(std::vector<std::shared_ptr<ICEPair>> &pairs_,
            uint8_t components);

signals:

  // When IceSessionTester finishes, it sends a success/failure signal.
  void iceSuccess(std::vector<std::shared_ptr<ICEPair>>& streams);

  void iceFailure(std::vector<std::shared_ptr<ICEPair>>& candidates);

  // private signal
  // used to end eventloop responsible for one interface
  void endTesting();

public slots:

  // current implementation ends testing if all components of a candidate
  // have succeeded
  void componentSucceeded(std::shared_ptr<ICEPair> connection);

protected:
  // handles the overall flow of testing so rest of Kvazzup is not
  // waiting for ICE to finish
  virtual void run();

private:

  std::shared_ptr<ICECandidateTester> createCandidateTester(std::shared_ptr<ICEInfo> local);

  // wait until all components have succeeded or timeout has occured
  void waitForEndOfTesting(unsigned long timeoutMs);

  std::vector<std::shared_ptr<ICEPair>> pairs_;

  bool isController_;
  int timeoutMs_;

  uint8_t components_;

  // makes sure we don't check results while a component is finishing.
  QMutex nominated_mtx;

  // temporary storage for finished components. componentSucceeeded copies
  // the succeeded components to finished_.
  QMap<QString, QMap<uint8_t, std::shared_ptr<ICEPair>>> finished_;

  // currently the first pair to have all its components succeed is selected.
  // These are then these are copied here.
  std::vector<std::shared_ptr<ICEPair>> nominated_;

  std::chrono::time_point<std::chrono::system_clock> startTime_;
};
