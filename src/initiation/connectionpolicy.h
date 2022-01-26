#pragma once


#include <QString>
#include <QList>

/* This class is responsible for determining if the incoming connection
 * is allowed */

// TODO: Implement this class

class ConnectionPolicy
{
public:
  ConnectionPolicy();

  bool isAllowed(QString localUsername, QString theirUsername, QString serverAddress,
                 QString contactAddress);

private:
  void initializePolicy();

  QList<QString> blockedUsers_;
  QList<QString> blockedIPs_;
};

