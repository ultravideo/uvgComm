#pragma once

#include <QMainWindow>

#include "filtergraph.h"

class StatisticsWindow;

namespace Ui {
class CallWindow;
}

class CallWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit CallWindow(QWidget *parent, uint16_t width, uint16_t height);
  ~CallWindow();

  void startStream();

  void closeEvent(QCloseEvent *event);

public slots:
   void addParticipant();
   void openStatistics(); // Opens statistics window

private:
  Ui::CallWindow *ui_;
  StatisticsWindow *stats_;

  FilterGraph fg_;

  QTimer *timer_; // for GUI update

  // dynamic videowidget adding to layout
  int row_;
  int column_;

  QSize currentResolution_;
};
