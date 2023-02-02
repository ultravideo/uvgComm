#pragma once

#include "icetypes.h"
#include "udpserver.h"

#include <QList>
#include <QHostAddress>
#include <QMutex>

class ICEPairTester;

/* Tests one local candidate against all remote candidates using
 * ICEPairTester for each tested connection */

class ICECandidateTester : public QObject
{
  Q_OBJECT
public:
  ICECandidateTester();

  // bind to port this tester is responsible for
  bool bindInterface(QHostAddress interface, quint16 port);

  // add one candidate we expect to get messages from.
  void addCandidate(std::shared_ptr<ICEPair> pair)
  {
    pairs_.push_back(pair);
  }

  // start sending of STUN binding requests for all added candidates
  // each candidate will have its own thread.
  void startTestingPairs(bool controller);

  // stops all testing
  void endTests();

  // nominate selected pairs as controller. Does separate nomination
  // for all components. Note that this concerns components of one candidate
  // where as startTestingPairs is concerned with one interface/port pair
  // of all candidates
  bool performNomination(QList<std::shared_ptr<ICEPair> > &nominated);

signals:
  void controllerPairFound(std::shared_ptr<ICEPair> connection);
  void controlleeNominationDone(std::shared_ptr<ICEPair> connection);

private slots:

  // finds which pair this packet belongs to
  void routeDatagram(QNetworkDatagram message);

private:

  UDPServer udp_;
  QList<std::shared_ptr<ICEPair>> pairs_;

  std::vector<std::shared_ptr<ICEPairTester>> workerThreads_;

  QMap<QString, QMap<quint16, std::shared_ptr<ICEPairTester>>> listeners_;

  QMutex listenerMutex_;
};
