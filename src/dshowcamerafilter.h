#pragma once

#include "filter.h"
#include "dshow/capture_interface.h"

class DShowCameraFilter : public Filter
{
public:
  DShowCameraFilter(QString id, StatisticsInterface *stats);

  void init();

  virtual void run();
  virtual void stop();

  virtual void process(){}
private:

  char **devices;
  deviceCapability *list_;

  unsigned int deviceID_;
  unsigned int settingsID_;

  bool run_;

};
