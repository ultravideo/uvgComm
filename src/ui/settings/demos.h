#pragma once

#include "qabstractbutton.h"
#include "ui/gui/videowidget.h"

#include <QDialog>
#include <QSettings>

namespace Ui {
class AutomaticSettings;
}

class Demos : public QDialog
{
  Q_OBJECT

public:
  explicit Demos(QWidget *parent = nullptr);
  ~Demos();

  void init();

  VideoWidget* getRoiSelfView();

  virtual void show();

public slots:
  void finished();

  void tabChanged(int index);

  void reset();

  void updateVideoConfig();
  void updateConfigAndReset(int i);
  void updateConfig(int i);

  void radioButton(QAbstractButton * button);

  void selectObject(int index);

signals:
  void updateAutomaticSettings();
  void updateVideoSettings();
  void hidden();

protected:

  // if user closes the window
  void closeEvent(QCloseEvent *event);

private:
  QSize getSettingsResolution();

  void activateROI();
  void disableROI();

  Ui::AutomaticSettings *ui_;

  int previousBitrate_;

  int lastTabIndex_;
};
