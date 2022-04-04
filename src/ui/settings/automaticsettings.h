#pragma once

#include "roiarea.h"

#include <QDialog>

namespace Ui {
class AutomaticSettings;
}

class AutomaticSettings : public QDialog
{
  Q_OBJECT

public:
  explicit AutomaticSettings(QWidget *parent = nullptr);
  ~AutomaticSettings();

public slots:
  void finished();

  void showROI();

signals:
  void updateAutomaticSettings();
  void hidden();

private:
  Ui::AutomaticSettings *ui;

  RoiArea roi_;
};
