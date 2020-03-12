#pragma once

#include "conferenceview.h"
#include "ui/settings/settings.h"
#include "contactlist.h"

#include <QMainWindow>

// The main Call window. It is also responsible for coordinating other GUI components.
// The user inputs are directed to call manager and call manager issues changes to GUI
// as necessary. Someday the separation of callwindow and uimanager may be necessary, but
// currently there is not enough functionality on the callwindow itself.

class StatisticsWindow;
class StatisticsInterface;
class ServerStatusView;


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

  // sessionID identifies the view slot
  void displayOutgoingCall(uint32_t sessionID, QString name);
  void displayRinging(uint32_t sessionID);
  void displayIncomingCall(uint32_t sessionID, QString caller);

  // adds video stream to view
  void addVideoStream(uint32_t sessionID);

  // removes caller from view
  void removeParticipant(uint32_t sessionID);

  void removeWithMessage(uint32_t sessionID, QString message, bool temporaryMessage);

  // set GUI to reflect state
  void setMicState(bool on);
  void setCameraState(bool on);

  // if user closes the window
  void closeEvent(QCloseEvent *event);

  // viewfactory for creating video views.
  std::shared_ptr<VideoviewFactory> getViewFactory() const
  {
    return viewFactory_;
  }

  ServerStatusView* getStatusView()
  {
    return &settingsView_;
  }

signals:

  // signals for informing the program of users wishes
  void settingsChanged();
  void micStateSwitch();
  void cameraStateSwitch();
  void shareStateSwitch();
  void endCall();
  void closed();

  // user reactions to incoming call.
  void callAccepted(uint32_t sessionID);
  void callRejected(uint32_t sessionID);
  void callCancelled(uint32_t sessionID);

public slots:

  // for processing GUI messages that mostly affect GUI elements.
  void addContact();
  void openStatistics(); // Opens statistics window

  // buttons are called automatically when named like this
  void on_settings_button_clicked();
  void on_about_clicked();

  void changedSIPText(const QString &text);

private:

  // helper for setting icons to buttons.
  void initButton(QString iconPath, QSize size, QSize iconSize, QPushButton* button);

  Ui::CallWindow *ui_;

  std::shared_ptr<VideoviewFactory> viewFactory_;

  Settings settingsView_;
  StatisticsWindow *statsWindow_;

  Ui::AboutWidget* aboutWidget_;
  QWidget about_;

  ConferenceView conference_;

  ContactList contacts_;
  ParticipantInterface* partInt_;

  QTimer *timer_; // for GUI update
};
