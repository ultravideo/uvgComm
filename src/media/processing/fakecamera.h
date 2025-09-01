#pragma once

#include "filter.h"

#include <QFile>
#include <QString>
#include <QSize>

class FakeCamera : public Filter
{
public:
  FakeCamera(QString id, StatisticsInterface* stats,
             std::shared_ptr<ResourceAllocator> hwResources);
  ~FakeCamera();

  // open the file
  virtual bool init();

  // close the file
  void uninit();

  virtual void start();
  virtual void stop();

  virtual void updateSettings();

protected:
  void process();


private:
  QFile file_;

  QSize resolution_;
  int framerate_;

  bool running_ = false;
};
