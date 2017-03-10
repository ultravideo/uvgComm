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

public slots:
   void addParticipant();
   void openStatistics(); // Opens statistics window

   void micState();
   void cameraState();

   void incomingCall(QString callID, QString caller);
   void callOurselves();

   void acceptCall();
   void rejectCall();

private:

  void createParticipant(QString ip_str, QString port_str);
  void hideLabel();

  Ui::CallWindow *ui_;
  Ui::CallerWidget *ui_widget_;

  StatisticsWindow *stats_;
  QWidget* callingWidget_;

  CallNegotiation call_neg_;
  CallManager call_;

  QTimer *timer_; // for GUI update

  // dynamic videowidget adding to layout
  int row_;
  int column_;

  QSize currentResolution_;

  uint16_t portsOpen_;

  QString ip_;
  QString port_;
};
