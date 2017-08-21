#pragma once

#include "callmanager.h"
#include "callnegotiation.h"
#include "conferenceview.h"
#include "settings.h"
#include "contactlist.h"
#include "participantinterface.h"

#include <QMainWindow>
#include <QMutex>

// This class is the heart of this software. It is responsible for directing
// all the other classes based on user input.

// TODO separate Call logic from UI ( split this class in two classes,
// one handling ui and one handling all of call logic )

// The main Call window. It is also responsible for coordinating the GUI components.
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

  void setMicState(bool on);
  void setCameraState(bool on);

public slots:
  void addContact();

  void openStatistics(); // Opens statistics window

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

private:

  //void createParticipant(QString& callID, const std::shared_ptr<SDPMessageInfo> peerInfo,
  //                       const std::shared_ptr<SDPMessageInfo> localInfo);

  Ui::CallWindow *ui_;

  Settings settingsView_;
  StatisticsWindow *stats_;

  Ui::AboutWidget* aboutWidget_;
  QWidget about_;

  QMutex conferenceMutex_;
  ConferenceView conference_;

  ContactList contacts_;
  CallManager manager_;
  CallNegotiation callNeg_;

  QTimer *timer_; // for GUI update
};
