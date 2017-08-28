#include "callwindow.h"
#include "ui_callwindow.h"

#include "ui_callingwidget.h"
#include "ui_about.h"

#include "statisticswindow.h"

#include <QCloseEvent>
#include <QTimer>
#include <QMetaType>
#include <QDebug>

CallWindow::CallWindow(QWidget *parent):
  QMainWindow(parent),
  ui_(new Ui::CallWindow),
  settingsView_(),
  statsWindow_(NULL),
  conference_(this),
    partInt_(NULL),
  timer_(new QTimer(this))
{}

CallWindow::~CallWindow()
{
  timer_->stop();
  delete timer_;
  if(statsWindow_)
  {
    statsWindow_->close();
    delete statsWindow_;
  }
  delete ui_;
}

void CallWindow::init(ParticipantInterface *partInt)
{
  partInt_ = partInt;

  ui_->setupUi(this);

  // GUI updates are handled solely by timer
  timer_->setInterval(30);
  timer_->setSingleShot(false);
  connect(timer_, SIGNAL(timeout()), ui_->participants, SLOT(update()));
  connect(timer_, SIGNAL(timeout()), ui_->SelfView, SLOT(update()));
  timer_->start();

  setWindowTitle("Kvazzup");

  contacts_.initializeList(ui_->contactList, partInt_);

  aboutWidget_ = new Ui::AboutWidget;
  aboutWidget_->setupUi(&about_);

  // TODO: I don't know why this is required.
  qRegisterMetaType<QVector<int> >("QVector<int>");


  QObject::connect(&settingsView_, SIGNAL(settingsChanged()),
                   this, SIGNAL(settingsChanged()));

  QObject::connect(ui_->mic, SIGNAL(clicked()),
                   this, SIGNAL(micStateSwitch()));

  QObject::connect(ui_->camera, SIGNAL(clicked()),
                   this, SIGNAL(cameraStateSwitch()));

  QObject::connect(ui_->EndCallButton, SIGNAL(clicked()),
                   this, SIGNAL(endCall()));

  QObject::connect(ui_->actionClose, SIGNAL(triggered()),
                   this, SIGNAL(closed()));

  QMainWindow::show();

  // TODO: move to conferenceview somehow
  Ui::CallerWidget *ui_widget = new Ui::CallerWidget;
  QWidget* holderWidget = new QWidget;
  ui_widget->setupUi(holderWidget);
  connect(ui_widget->AcceptButton, SIGNAL(clicked()), this, SLOT(acceptCall()));
  connect(ui_widget->DeclineButton, SIGNAL(clicked()), this, SLOT(rejectCall()));

  conferenceMutex_.lock();
  conference_.init(ui_->participantLayout, ui_->participants, ui_widget, holderWidget);
  conferenceMutex_.unlock();
}

StatisticsInterface* CallWindow::createStatsWindow()
{
  statsWindow_ = new StatisticsWindow(this);
  connect(timer_, SIGNAL(timeout()), statsWindow_, SLOT(update()));
  return statsWindow_;
}

VideoWidget* CallWindow::getSelfDisplay()
{
  return ui_->SelfView;
}

void CallWindow::addContact()
{
  contacts_.addContact(partInt_, ui_->peerName->text(), "anonymous", ui_->ip->text());
}

void CallWindow::displayOutgoingCall(QString callID, QString name)
{
  conferenceMutex_.lock();
  conference_.callingTo(callID, name); // TODO get name from contact list
  conferenceMutex_.unlock();
}

void CallWindow::displayIncomingCall(QString callID, QString caller)
{
  conferenceMutex_.lock();
  conference_.incomingCall(callID, caller);
  conferenceMutex_.unlock();
}

void CallWindow::displayRinging(QString callID)
{
  qDebug() << "call is ringing. TODO: display it to user";
}

void CallWindow::acceptCall()
{
  qDebug() << "We accepted";
  conferenceMutex_.lock();
  QString callID = conference_.acceptNewest();
  conferenceMutex_.unlock();
  emit callAccepted(callID);
}

void CallWindow::rejectCall()
{
  qDebug() << "We rejected";
  conferenceMutex_.lock();
  QString callID = conference_.rejectNewest();
  conferenceMutex_.unlock();

  emit callRejected(callID);
}

void CallWindow::openStatistics()
{
  if(statsWindow_)
  {
    statsWindow_->show();
  }
  else
  {
    qDebug() << "WARNING: No stats window class initiated";
  }
}

void CallWindow::closeEvent(QCloseEvent *event)
{
  emit closed();
  statsWindow_->hide();
  statsWindow_->finished(0);

  QMainWindow::closeEvent(event);
}

VideoWidget* CallWindow::addVideoStream(QString callID)
{
  conferenceMutex_.lock();
  VideoWidget* view = conference_.addVideoStream(callID);
  conferenceMutex_.unlock();
  return view;
}

void CallWindow::setMicState(bool on)
{
  if(on)
  {
    ui_->mic->setText("Mic off");
  }
  else
  {
    ui_->mic->setText("Mic on");
  }
}

void CallWindow::setCameraState(bool on)
{
  if(on)
  {
    ui_->camera->setText("Camera off");
  }
  else
  {
    ui_->camera->setText("Camera on");
  }
}

void CallWindow::removeParticipant(QString callID)
{
  conferenceMutex_.lock();
  conference_.removeCaller(callID);
  conferenceMutex_.unlock();
}

void CallWindow::on_settings_clicked()
{
  settingsView_.showBasicSettings();
}

void CallWindow::on_advanced_settings_clicked()
{
  settingsView_.showAdvancedSettings();
}

void CallWindow::on_about_clicked()
{
  about_.show();
}

void CallWindow::clearConferenceView()
{
  conferenceMutex_.lock();
  conference_.close();
  conferenceMutex_.unlock();
}
