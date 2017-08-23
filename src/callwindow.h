#pragma once

#include "conferenceview.h"
#include "settings.h"
#include "contactlist.h"

#include <QMainWindow>
#include <QMutex>

// The main Call window. It is also responsible for coordinating other GUI components.
// The user inputs are directed to call manager and call manager issues changes to GUI
// as necessary

class StatisticsWindow;
class StatisticsInterface;

namespace Ui {
class CallWindow;
class CallerWidget;
class AboutWidget;
}

class CallWindow : public QMainWindow
{
  Q_OBJECT
public:
  explicit CallWindow(QWidget *parent);
  ~CallWindow();

  void init();

  // Connects all functions that are called when the
  // user interacts with the program
  void registerGUIEndpoints(ParticipantInterface *partInt);

  VideoWidget* getSelfDisplay();

  StatisticsInterface* createStatsWindow();

  void callingTo(QString callID);

  VideoWidget* addVideoStream(QString callID);

  void incomingCallNotification(QString callID, QString caller);

  void closeEvent(QCloseEvent *event);

  void setMicState(bool on);
  void setCameraState(bool on);

  // UI messages
  void acceptCall();
  void rejectCall();


signals:

  void closed();

  void settingsChanged();

public slots:

  void forwardSettingsUpdate();

  void addContact();

  void openStatistics(); // Opens statistics window

  void ringing(QString callID);

  void ourCallRejected(QString callID);


  void endCall(QString callID, QString ip);
  void endAllCalls();

private slots:
  void on_settings_clicked();

  void on_advanced_settings_clicked();

  void on_about_clicked();

private:

  //void createParticipant(QString& callID, const std::shared_ptr<SDPMessageInfo> peerInfo,
  //                       const std::shared_ptr<SDPMessageInfo> localInfo);

  Ui::CallWindow *ui_;

  Settings settingsView_;
  StatisticsWindow *statsWindow_;

  Ui::AboutWidget* aboutWidget_;
  QWidget about_;

  QMutex conferenceMutex_;
  ConferenceView conference_;

  ParticipantInterface* partInt_;
  ContactList contacts_;

  QTimer *timer_; // for GUI update
};
