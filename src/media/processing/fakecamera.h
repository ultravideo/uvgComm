#pragma once

#include "filter.h"

#include <QFile>
#include <QSize>
#include <QString>
#include <QTimer>

class FakeCamera : public Filter
{
public:
  FakeCamera(QString id, StatisticsInterface* stats,
             std::shared_ptr<ResourceAllocator> hwResources);
  ~FakeCamera();

  virtual void start();
  virtual void stop();


  virtual bool init();

  virtual void updateSettings();

protected:
  void process();

private slots:
  void sendFrame();

private:

  QTimer sendTimer_;
  QFile file_;
  QSize resolution_;
  int framerate_;

  bool running_;
};
