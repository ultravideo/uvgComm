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
   void pause();
   void micState();
   void cameraState();

private:
  Ui::CallWindow *ui_;
  StatisticsWindow *stats_;

  FilterGraph fg_;
  bool filterIniated_;


  QTimer *timer_; // for GUI update

  // dynamic videowidget adding to layout
  int row_;
  int column_;

  bool running_;

  bool mic_;
  bool camera_;

  QSize currentResolution_;

  uint16_t portsOpen_;
};
