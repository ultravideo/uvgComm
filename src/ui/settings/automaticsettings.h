#pragma once

#include "ui/gui/videowidget.h"

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

  virtual void show();

public slots:
  void finished();

  void tabChanged(int index);

  void reset();

signals:
  void updateAutomaticSettings();
  void hidden();

private:

  void activateROI();
  void disableROI();


  Ui::AutomaticSettings *ui_;

  QSettings settings_;
};
