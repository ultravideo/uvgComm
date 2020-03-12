#pragma once

#include "filter.h"

#include <QTimer>

class ScreenShareFilter : public Filter
{
  Q_OBJECT
public:
  ScreenShareFilter(QString id, StatisticsInterface* stats);
  virtual bool init();

protected:

  virtual void process();

private slots:

  void sendScreen();

private:

  QTimer sendTimer_;
};
