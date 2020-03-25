#pragma once

#include "icetypes.h"
#include "udpserver.h"

#include <QList>
#include <QHostAddress>


class InterfaceTester : public QObject
{
  Q_OBJECT
public:
  InterfaceTester();

  bool bindInterface(QHostAddress interface, quint16 port);

  void addCandidate(std::shared_ptr<ICEPair> pair)
  {
    pairs_.push_back(pair);
  }

  void startTestingPairs(bool controller);

  void endTests();

signals:
  void candidateFound(std::shared_ptr<ICEPair> connection);

private slots:
  void routeDatagram(QNetworkDatagram message);

private:

  void expectReplyFrom(std::shared_ptr<ConnectionTester> ct,
                       QString& address, quint16 port);

  UDPServer udp_;
  QList<std::shared_ptr<ICEPair>> pairs_;

  std::vector<std::shared_ptr<ConnectionTester>> workerThreads_;

  QMap<QString, QMap<quint16, std::shared_ptr<ConnectionTester>>> listeners_;
};
