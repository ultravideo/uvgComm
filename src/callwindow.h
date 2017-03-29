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
   void callOurselves(std::shared_ptr<SDPMessageInfo> info);

   void ringing(QString callID);

   void ourCallAccepted(QString CallID, std::shared_ptr<SDPMessageInfo> info);
   void ourCallRejected(QString callID);

   void theirCallNegotiated(std::shared_ptr<SDPMessageInfo> info);

   // UI messages
   void acceptCall();
   void rejectCall();

   void endCall(QString callID);

private:

  void createParticipant(QString ip_str, uint16_t port);
  void hideLabel();

  void processNextWaitingCall();

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

  struct CallInfo
  {
    QString callID;
    QString name;
  };

  QMutex callWaitingMutex_;
  QList<CallInfo> waitingCalls_;
  bool ringing_;
};
