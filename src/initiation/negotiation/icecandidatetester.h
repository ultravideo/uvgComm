#pragma once

#include "icetypes.h"
#include "udpserver.h"

#include <QList>
#include <QHostAddress>

class IcePairTester;

class IceCandidateTester : public QObject
{
  Q_OBJECT
public:
  IceCandidateTester();

  bool bindInterface(QHostAddress interface, quint16 port);

  void addCandidate(std::shared_ptr<ICEPair> pair)
  {
    pairs_.push_back(pair);
  }

  void startTestingPairs(bool controller);

  void endTests();

  bool performNomination(QList<std::shared_ptr<ICEPair> > &nominated);

signals:
  void controllerPairFound(std::shared_ptr<ICEPair> connection);
  void controlleeNominationDone(std::shared_ptr<ICEPair> connection);

private slots:
  void routeDatagram(QNetworkDatagram message);

private:

  void expectReplyFrom(std::shared_ptr<IcePairTester> ct,
                       QString& address, quint16 port);

  UDPServer udp_;
  QList<std::shared_ptr<ICEPair>> pairs_;

  std::vector<std::shared_ptr<IcePairTester>> workerThreads_;

  QMap<QString, QMap<quint16, std::shared_ptr<IcePairTester>>> listeners_;
};
