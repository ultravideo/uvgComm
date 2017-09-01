#include "callwindow.h"

#include "ui_callwindow.h"
#include "ui_about.h"

#include "statisticswindow.h"

#include <QCloseEvent>
#include <QTimer>
#include <QMetaType>
#include <QDebug>
#include <QDir>

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

  // I don't know why this is required.
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

  connect(&conference_, SIGNAL(acceptCall(QString)), this, SIGNAL(callAccepted(QString)));
  connect(&conference_, SIGNAL(rejectCall(QString)), this, SIGNAL(callRejected(QString)));

  conference_.init(ui_->participantLayout, ui_->participants);

  initButton(QDir::currentPath() + "/icons/plus.svg", QSize(60,60), QSize(35,35), ui_->addContact);
  initButton(QDir::currentPath() + "/icons/settings.svg", QSize(60,60), QSize(35,35), ui_->settings);
  initButton(QDir::currentPath() + "/icons/photo-camera.svg", QSize(60,60), QSize(35,35), ui_->camera);
  initButton(QDir::currentPath() + "/icons/microphone.svg", QSize(60,60), QSize(35,35), ui_->mic);
  initButton(QDir::currentPath() + "/icons/end_call.svg", QSize(60,60), QSize(35,35), ui_->EndCallButton);
}

void CallWindow::initButton(QString iconPath, QSize size, QSize iconSize, QPushButton* button)
{
  QPixmap pixmap(iconPath);
  if(!pixmap.isNull())
  {
    QIcon ButtonIcon(pixmap);
    button->setIcon(ButtonIcon);
    button->setText("");

    button->setMaximumSize(size);
    button->setMinimumSize(size);
    button->setIconSize(iconSize);
  }
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
  conference_.callingTo(callID, name); // TODO get name from contact list
}

void CallWindow::displayIncomingCall(QString callID, QString caller)
{
  conference_.incomingCall(callID, caller);
}

void CallWindow::displayRinging(QString callID)
{
  conference_.ringing(callID);
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
  VideoWidget* view = conference_.addVideoStream(callID);
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
  conference_.removeCaller(callID);
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
  conference_.close();
}
