#pragma once

#include "gui/guimessage.h"
#include "ui/settings/settings.h"
#include "gui/callwindow.h"

class StatisticsWindow;
class StatisticsInterface;
class ServerStatusView;
class VideoviewFactory;

namespace Ui {
class AboutWidget;
}

/*  This class responsible for coordinating GUI components and user interaction
 *  with Kvazzup.
 */

class UIManager : public QObject
{
  Q_OBJECT
public:
  UIManager();
  ~UIManager();

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

  void removeWithMessage(uint32_t sessionID, QString message,
                         bool temporaryMessage);

  // viewfactory for creating video views.
  std::shared_ptr<VideoviewFactory> getViewFactory() const
  {
    return viewFactory_;
  }

  ServerStatusView* getStatusView()
  {
    return &settingsView_;
  }

  void showICEFailedMessage();
  void showCryptoMissingMessage();
  void showZRTPFailedMessage(QString sessionID);

  void showMainWindow();

signals:

  void updateCallSettings();
  void updateVideoSettings();
  void updateAudioSettings();

  void endCall();
  void quit();

  // user reactions to incoming call.
  void callAccepted(uint32_t sessionID);
  void callRejected(uint32_t sessionID);
  void callCancelled(uint32_t sessionID);

public slots:

  void videoSourceChanged(bool camera, bool screenShare);
  void audioSourceChanged(bool mic);

  void showStatistics(); // Opens statistics window
  void showSettings();
  void showAbout();

private:

  CallWindow window_;

  std::shared_ptr<VideoviewFactory> viewFactory_;

  Settings settingsView_;
  StatisticsWindow *statsWindow_;

  Ui::AboutWidget* aboutWidget_;
  QWidget about_;

  QTimer *timer_; // for GUI update
  GUIMessage mesg_;
};
