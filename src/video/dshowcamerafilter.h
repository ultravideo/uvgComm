#pragma once
#include "filter.h"

struct deviceCapability;

class DShowCameraFilter : public Filter
{
public:
  DShowCameraFilter(QString id, StatisticsInterface *stats);
  ~DShowCameraFilter();


  virtual bool init();
  virtual void start();
  virtual void run();
  virtual void stop();

  virtual void process(){}

  virtual void updateSettings();

private:
  deviceCapability *list_;
  int deviceID_;
  int capabilityID_;

  //DataType type_;

  bool run_;

  bool exited_;
};
