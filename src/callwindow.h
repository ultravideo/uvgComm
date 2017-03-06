#pragma once

#include <QMainWindow>

#include "callmanager.h"
#include "callnegotiation.h"

class StatisticsWindow;

namespace Ui {
class CallWindow;
class CallerWidget;
}

class CallWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit CallWindow(QWidget *parent, uint16_t width, uint16_t height);
  ~CallWindow();

  void startStream();

  void closeEvent(QCloseEvent *event);

  bool isRunning()
  {
    return running_;
  }

public slots:
   void addParticipant();
   void openStatistics(); // Opens statistics window
   void micState();
   void cameraState();

   void incomingCall(QString caller);


private:
  Ui::CallWindow *ui_;
  Ui::CallerWidget *widget_;

  StatisticsWindow *stats_;

  CallManager call_;

  CallNegotiation call_neg_;

  QWidget* callingWidget_;

  QTimer *timer_; // for GUI update

  // dynamic videowidget adding to layout
  int row_;
  int column_;

  QSize currentResolution_;

  uint16_t portsOpen_;

  bool running_;
};
