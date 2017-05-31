#pragma once
#include "mediamanager.h"
#include "callnegotiation.h"
#include "conferenceview.h"
#include "settings.h"
#include "contactlist.h"
#include "participantinterface.h"

#include <QMainWindow>

// This class is the heart of this software. It is responsible for directing
// all the other classes based on user input.

// TODO separate Call logic from UI ( split this class in two classes,
// one handling ui and one handling all of call logic )

class StatisticsWindow;

namespace Ui {
class CallWindow;
class CallerWidget;
class AboutWidget;
}

class CallWindow : public QMainWindow, public ParticipantInterface
{
  Q_OBJECT
public:
  explicit CallWindow(QWidget *parent);
  ~CallWindow();

  void startStream();

  virtual void callToParticipant(QString name, QString username, QString ip);
  virtual void chatWithParticipant(QString name, QString username, QString ip);

  void closeEvent(QCloseEvent *event);

public slots:
  void addContact();

  void openStatistics(); // Opens statistics window

  void micState();
  void cameraState();

  void incomingCall(QString callID, QString caller);
  void callOurselves(QString callID, std::shared_ptr<SDPMessageInfo> info);

  void ringing(QString callID);

  void ourCallRejected(QString callID);

  void callNegotiated(QString callID, std::shared_ptr<SDPMessageInfo> peerInfo,
                     std::shared_ptr<SDPMessageInfo> localInfo);

  // UI messages
  void acceptCall();
  void rejectCall();

  void endCall(QString callID, QString ip);
  void endAllCalls();

private slots:
  void on_settings_clicked();

  void on_advanced_settings_clicked();

  void on_about_clicked();

  void recordChangedSettings();

private:

  void createParticipant(QString& callID, const std::shared_ptr<SDPMessageInfo> peerInfo,
                         const std::shared_ptr<SDPMessageInfo> localInfo);

  Ui::CallWindow *ui_;

  Settings settingsView_;
  StatisticsWindow *stats_;

  Ui::AboutWidget* aboutWidget_;
  QWidget about_;

  QMutex conferenceMutex_;
  ConferenceView conference_;

  ContactList contacts_;
  CallNegotiation callNeg_;
  MediaManager media_;

  QTimer *timer_; // for GUI update

  uint16_t portsOpen_;
};
