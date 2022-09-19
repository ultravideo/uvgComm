#pragma once

#include "filter.h"

#include <QTimer>
#include <QSize>

class ScreenShareFilter : public Filter
{
  Q_OBJECT
public:
  ScreenShareFilter(QString id, StatisticsInterface* stats,
                    std::shared_ptr<ResourceAllocator> hwResources);
  virtual bool init();

  virtual void updateSettings();

protected:

  virtual void process();

private slots:

  void sendScreen();

private:

  QTimer sendTimer_;

  int32_t framerateNumerator_;
  int32_t framerateDenominator_;
  QSize currentResolution_;

  int screenID_;
};
