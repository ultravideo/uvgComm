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

  void init(ParticipantInterface *partInt);

  // Connects all functions that are called when the
  // user interacts with the program
  void registerGUIEndpoints();

  StatisticsInterface* createStatsWindow();

  void displayOutgoingCall(QString callID);
  void displayRinging(QString callID);
  void displayIncomingCall(QString callID, QString caller);

  VideoWidget* getSelfDisplay();
  VideoWidget* addVideoStream(QString callID);

  void incomingCallNotification(QString callID, QString caller);

  void closeEvent(QCloseEvent *event);

  void setMicState(bool on);
  void setCameraState(bool on);
  void clearConferenceView();

  // UI messages
  void acceptCall();
  void rejectCall();

signals:

  // maybe addcontact too, after the settings has been reorganized.
  void settingsChanged();
  void micStateSwitch();
  void cameraStateSwitch();
  void endCall();
  void closed();

  //void kickParticipant();

public slots:

  void addContact();

  void openStatistics(); // Opens statistics window

  void ourCallRejected(QString callID);

  void endCall(QString callID, QString ip);

  void on_settings_clicked();
  void on_advanced_settings_clicked();
  void on_about_clicked();

private:

  Ui::CallWindow *ui_;

  Settings settingsView_;
  StatisticsWindow *statsWindow_;

  Ui::AboutWidget* aboutWidget_;
  QWidget about_;

  QMutex conferenceMutex_;
  ConferenceView conference_;

  ContactList contacts_;
  ParticipantInterface* partInt_;

  QTimer *timer_; // for GUI update
};
