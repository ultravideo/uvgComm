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

  // functions for managing the GUI
  StatisticsInterface* createStatsWindow();

  VideoWidget* getSelfDisplay();
  void displayOutgoingCall(QString callID, QString name);
  void displayRinging(QString callID);
  void displayIncomingCall(QString callID, QString caller);

  VideoWidget* addVideoStream(QString callID);
  void removeParticipant(QString callID);

  void setMicState(bool on);
  void setCameraState(bool on);
  void clearConferenceView();

  void closeEvent(QCloseEvent *event);

signals:

  // signals for informing the program of users wishes
  void settingsChanged();
  void micStateSwitch();
  void cameraStateSwitch();
  void endCall();
  void closed();

  void callAccepted(QString callID);
  void callRejected(QString callID);

  //void kickParticipant();

public slots:

  // for processing GUI messages that mostly affect GUI elements.
  void addContact();
  void openStatistics(); // Opens statistics window
  void on_settings_clicked();
  void on_advanced_settings_clicked();
  void on_about_clicked();

  // responses to GUI question of incoming call. Also emit signals
  void acceptCall();
  void rejectCall();

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
