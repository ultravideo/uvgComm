#pragma once
#include <QString>

class ServerStatusView
{
 public:

  virtual ~ServerStatusView(){}

  virtual void updateServerStatus(QString status) = 0;
};
