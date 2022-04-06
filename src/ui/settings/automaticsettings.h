#pragma once

#include "roiarea.h"

#include <QDialog>
#include <QSettings>

namespace Ui {
class AutomaticSettings;
}

class AutomaticSettings : public QDialog
{
  Q_OBJECT

public:
  explicit AutomaticSettings(QWidget *parent = nullptr);
  ~AutomaticSettings();

    VideoWidget* getRoiSelfView();

public slots:
  void finished();

  void showROI();
  void roiAreaClosed();

signals:
  void updateAutomaticSettings();
  void hidden();



private:
  Ui::AutomaticSettings *ui;

  RoiArea roi_;

  QSettings settings_;
};
