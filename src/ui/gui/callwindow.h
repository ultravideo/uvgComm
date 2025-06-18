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
class SDPMediaParticipant;

// The main Call window.

class CallWindow : public QMainWindow
{
  Q_OBJECT
public:
  explicit CallWindow(QWidget *parent);
  ~CallWindow();

  VideoWidget *getSelfView() const;

  void init(ParticipantInterface *partInt);

  // sessionID identifies the view slot
  void displayOutgoingCall(uint32_t sessionID, QString name);
  void displayRinging(uint32_t sessionID);
  void displayIncomingCall(uint32_t sessionID, QString caller);

  // adds video stream to view
  void callStarted(std::shared_ptr<VideoviewFactory> viewFactory,
                   uint32_t sessionID,
                   QStringList names,
                   const std::map<QString, MediaSource> &sources);

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

  void removeLayout(LayoutID layoutID);

  void layoutAccepts(LayoutID layoutID);
  void layoutRejects(LayoutID layoutID);
  void layoutCancels(LayoutID layoutID);

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

  void initializeMediaStates();

  void checkID(uint32_t sessionID);
  bool layoutExists(uint32_t sessionID);

  uint32_t layoutIDToSessionID(LayoutID layoutID);

  void cleanUp();

  bool getTempLayoutID(LayoutID& id, uint32_t sessionID);

  void removeExpiredLayouts();

  Ui::CallWindow *ui_;

  ConferenceView conference_;

  ContactList contacts_;
  ParticipantInterface* partInt_;

  QTimer removeLayoutTimer_;
  QList<LayoutID> expiringLayouts_;

  // key is sessionID, used to track incoming/outgoing call view in the session
  std::map<uint32_t, LayoutID> temporaryLayoutIDs_;

  // key is sessionID, used to store layoutIDs for convenient deletion
  std::map<uint32_t, std::vector<LayoutID>> layoutIDs_;

  // stores the video layoutID for each cname
  std::map<uint32_t, std::map<QString, LayoutID>> sessionCnameToLayoutID_;
};
