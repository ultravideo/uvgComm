#pragma once

#include "conferenceview.h"
#include "contactlist.h"

#include <QMainWindow>
#include <QPushButton>


namespace Ui {
class CallWindow;
class CallerWidget;
}

class VideoWidget;
class VideoInterface;

// The main Call window.

class CallWindow : public QMainWindow
{
  Q_OBJECT
public:
  explicit CallWindow(QWidget *parent);
  ~CallWindow();

  QList<VideoInterface*> getSelfVideos () const;

  void init(ParticipantInterface *partInt, VideoWidget *settingsVideo);

  // sessionID identifies the view slot
  void displayOutgoingCall(uint32_t sessionID, QString name);
  void displayRinging(uint32_t sessionID);
  void displayIncomingCall(uint32_t sessionID, QString caller);

  // adds video stream to view
  VideoInterface *callStarted(uint32_t sessionID, bool videoEnabled, bool audioEnabled,
                              QString name);

  // removes caller from view
  void removeParticipant(uint32_t sessionID);

  void removeWithMessage(uint32_t sessionID, QString message,
                         bool temporaryMessage);

signals:

  void endCall();
  void closed();
  void quit();

  // user reactions to incoming call.
  void callAccepted(uint32_t sessionID);
  void callRejected(uint32_t sessionID);
  void callCancelled(uint32_t sessionID);

  void videoSourceChanged(bool camera, bool screenShare);
  void audioSourceChanged(bool mic);

  void openStatistics();
  void openSettings();
  void openAbout();

public slots:

  // for processing GUI messages that mostly affect GUI elements.
  void addContact();

  void addContactButton();

  void screensShareButton();
  void micButton(bool checked);
  void cameraButton(bool checked);

  void changedSIPText(const QString &text);

  void restoreCallUI();

  void removeLayout(uint32_t layoutID);

  void layoutAccepts(uint32_t layoutID);
  void layoutRejects(uint32_t layoutID);
  void layoutCancels(uint32_t layoutID);

private slots:
  void expireLayouts();

protected:

  // if user closes the window
  void closeEvent(QCloseEvent *event);

private:

  // set GUI to reflect state
  void setMicState(bool on);
  void setCameraState(bool on);
  void setScreenShareState(bool on);

  // helper for setting icons to buttons.
  void initButton(QString iconPath, QSize size, QSize iconSize,
                  QPushButton* button);

  void checkID(uint32_t sessionID);
  bool layoutExists(uint32_t sessionID);

  uint32_t layoutIDToSessionID(uint32_t layoutID);

  void cleanUp();

  Ui::CallWindow *ui_;

  ConferenceView conference_;

  // video views
  std::shared_ptr<VideoviewFactory> viewFactory_;

  ContactList contacts_;
  ParticipantInterface* partInt_;

  QTimer removeLayoutTimer_;
  QList<uint32_t> expiringLayouts_;

  // key is sessionID and vector contains all layoutIDs belonging to this sessionID
  std::map<uint32_t, std::vector<uint32_t>> layoutIDs_;
};
