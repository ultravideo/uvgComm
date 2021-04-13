#pragma once

#include "filter.h"

#include <QTimer>
#include <QSize>

class ScreenShareFilter : public Filter
{
  Q_OBJECT
public:
  ScreenShareFilter(QString id, StatisticsInterface* stats);
  virtual bool init();

  virtual void updateSettings();

protected:

  virtual void process();

private slots:

  void sendScreen();

private:

  QTimer sendTimer_;

  int currentFramerate_;
  QSize currentResolution_;
};
