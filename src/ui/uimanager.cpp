#include "uimanager.h"

#include "gui/statisticswindow.h"
#include "gui/videowidget.h"

#include "gui/videoviewfactory.h"

#include "logger.h"

#include "ui_about.h"

UIManager::UIManager():
  window_(nullptr),
  settingsView_(&window_),
  statsWindow_(nullptr),
  timer_(new QTimer(&window_))
{}


UIManager::~UIManager()
{
  timer_->stop();
  delete timer_;
  if(statsWindow_)
  {
    statsWindow_->close();
    delete statsWindow_;
  }
}


std::shared_ptr<VideoviewFactory> UIManager::getViewFactory() const
{
  return window_.getViewFactory();
}


void UIManager::init(ParticipantInterface *partInt)
{
  aboutWidget_ = new Ui::AboutWidget;
  aboutWidget_->setupUi(&about_);

  QObject::connect(&settingsView_, &Settings::updateCallSettings,
                   this,           &UIManager::updateCallSettings);

  QObject::connect(&settingsView_, &Settings::updateVideoSettings,
                   this,           &UIManager::updateVideoSettings);

  QObject::connect(&settingsView_, &Settings::updateAudioSettings,
                   this,           &UIManager::updateAudioSettings);

  QObject::connect(&settingsView_, &Settings::updateAutomaticSettings,
                   this,           &UIManager::updateAutomaticSettings);

  QObject::connect(&window_, &CallWindow::videoSourceChanged,
                   this, &UIManager::videoSourceChanged);

  QObject::connect(&window_, &CallWindow::audioSourceChanged,
                   this, &UIManager::audioSourceChanged);

  QObject::connect(&window_, &CallWindow::openSettings,
                   this, &UIManager::showSettings);

  QObject::connect(&window_, &CallWindow::closed,
                   this,     &UIManager::quit);

  QObject::connect(&window_, &CallWindow::quit,
                   this,     &UIManager::quit);

  QObject::connect(&window_, &CallWindow::callAccepted,
                   this,     &UIManager::callAccepted);

  QObject::connect(&window_, &CallWindow::callRejected,
                   this,     &UIManager::callRejected);

  QObject::connect(&window_, &CallWindow::callCancelled,
                   this,     &UIManager::callCancelled);

  QObject::connect(&window_, &CallWindow::openStatistics,
                   this,     &UIManager::showStatistics);
  QObject::connect(&window_, &CallWindow::openSettings,
                   this,     &UIManager::showSettings);
  QObject::connect(&window_, &CallWindow::openAbout,
                   this,     &UIManager::showAbout);

  QObject::connect(&window_, &CallWindow::endCall,
                   this,     &UIManager::endCall);

  settingsView_.init();

  window_.init(partInt);
}


void UIManager::updateServerStatus(QString status)
{
  settingsView_.updateServerStatus(status);
}


// functions for managing the GUI
StatisticsInterface* UIManager::createStatsWindow()
{
  Logger::getLogger()->printNormal(this, "Creating statistics window");

  statsWindow_ = new StatisticsWindow(&window_);

  // Stats GUI updates are handled solely by timer
  timer_->setInterval(200);
  timer_->setSingleShot(false);
  timer_->start();

  connect(timer_, SIGNAL(timeout()), statsWindow_, SLOT(update()));
  return statsWindow_;
}


// sessionID identifies the view slot
void UIManager::displayOutgoingCall(uint32_t sessionID, QString name)
{
  window_.displayOutgoingCall(sessionID, name);
}


void UIManager::displayRinging(uint32_t sessionID)
{
  window_.displayRinging(sessionID);
}


void UIManager::displayIncomingCall(uint32_t sessionID, QString caller)
{
  window_.displayIncomingCall(sessionID, caller);
}


// adds video stream to view
void UIManager::callStarted(uint32_t sessionID, bool videoEnabled, bool audioEnabled)
{
  window_.callStarted(sessionID, videoEnabled, audioEnabled);
}


// removes caller from view
void UIManager::removeParticipant(uint32_t sessionID)
{
  window_.removeParticipant(sessionID);
}


void UIManager::removeWithMessage(uint32_t sessionID, QString message,
                       bool temporaryMessage)
{
  window_.removeWithMessage(sessionID, message, temporaryMessage);
}


void UIManager::videoSourceChanged(bool camera, bool screenShare)
{
  settingsView_.setCameraState(camera);
  settingsView_.setScreenShareState(screenShare);

  emit updateVideoSettings();
}


void UIManager::audioSourceChanged(bool mic)
{
  settingsView_.setMicState(mic);
  emit updateAudioSettings();
}


void UIManager::showICEFailedMessage()
{
  mesg_.showError("Error: ICE Failed",
                  "ICE did not succeed to find a connection. "
                  "You may try again. If the issue persists, "
                  "please report the issue to Github if you are connected to the internet "
                  "and describe your network setup.");
}


void UIManager::showCryptoMissingMessage()
{
  mesg_.showWarning("Warning: Encryption not possible",
                    "Crypto++ has not been included in both Kvazzup and uvgRTP.");
}


void UIManager::showZRTPFailedMessage(QString sessionID)
{
  mesg_.showError("Error: ZRTP handshake has failed for session " + sessionID,
                  "Could not exchange encryption keys.");
}


void UIManager::showMainWindow()
{
  window_.show();
}


void UIManager::showStatistics()
{
  if(statsWindow_)
  {
    statsWindow_->show();
  }
  else
  {
    Logger::getLogger()->printProgramWarning(this,
                                             "No stats window class initiated when opening it");
  }
}


void UIManager::showSettings()
{
  settingsView_.show();
}


void UIManager::showAbout()
{
  about_.show();
}
