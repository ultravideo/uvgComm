#pragma once
#include <QString>

enum ServerStatus {DISCONNECTED, IN_PROCESS, BEHIND_NAT, REGISTERED, SERVER_FAILED};

class ServerStatusView
{
 public:

  virtual ~ServerStatusView(){}

  virtual void updateServerStatus(ServerStatus status) = 0;
};
