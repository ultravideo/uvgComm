#pragma once

#include "conferenceview.h"
#include "settings.h"
#include "contactlist.h"

#include <QMainWindow>

// The main Call window. It is also responsible for coordinating other GUI components.
// The user inputs are directed to call manager and call manager issues changes to GUI
// as necessary. Someday the separation of callwindow and uimanager may be necessary, but
// currently there is not enough functionality on the callwindow itself.

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

  // gets video widget from GUI
  VideoWidget* getSelfDisplay();

  // callID identifies the view slot
  void displayOutgoingCall(uint32_t sessionID, QString name);
  void displayRinging(uint32_t sessionID);
  void displayIncomingCall(uint32_t sessionID, QString caller);

  // adds video stream to view and returns created/atttached videowidget
  VideoWidget* addVideoStream(uint32_t sessionID);

  // removes caller from view
  void removeParticipant(uint32_t sessionID);
  void clearConferenceView();

  // set GUI to reflect state
  void setMicState(bool on);
  void setCameraState(bool on);

  void closeEvent(QCloseEvent *event);

signals:

  // signals for informing the program of users wishes
  void settingsChanged();
  void micStateSwitch();
  void cameraStateSwitch();
  void endCall();
  void closed();

  void callAccepted(uint32_t sessionID);
  void callRejected(uint32_t sessionID);

  //void kickParticipant();

public slots:

  // for processing GUI messages that mostly affect GUI elements.
  void addContact();
  void openStatistics(); // Opens statistics window
  void on_settings_clicked();
  void on_advanced_settings_clicked();
  void on_about_clicked();

private:

  void initButton(QString iconPath, QSize size, QSize iconSize, QPushButton* button);

  Ui::CallWindow *ui_;

  Settings settingsView_;
  StatisticsWindow *statsWindow_;

  Ui::AboutWidget* aboutWidget_;
  QWidget about_;

  ConferenceView conference_;

  ContactList contacts_;
  ParticipantInterface* partInt_;

  QTimer *timer_; // for GUI update
};
