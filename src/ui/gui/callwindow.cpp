#include "callwindow.h"

#include "ui_callwindow.h"
#include "ui_about.h"

#include "statisticswindow.h"
#include "videoviewfactory.h"

#include "common.h"
#include "settingskeys.h"

#include <QCloseEvent>
#include <QTimer>
#include <QMetaType>
#include <QDebug>
#include <QDir>


CallWindow::CallWindow(QWidget *parent):
  QMainWindow(parent),
  ui_(new Ui::CallWindow),
  viewFactory_(std::shared_ptr<VideoviewFactory>(new VideoviewFactory)),
  settingsView_(this),
  statsWindow_(nullptr),
  conference_(this),
  partInt_(nullptr),
  timer_(new QTimer(this))
{
  ui_->setupUi(this);
}

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

  ui_->Add_contact_widget->setVisible(false);

  viewFactory_->setSelfview(ui_->SelfView, ui_->SelfView);

  setWindowTitle("Kvazzup");

  contacts_.initializeList(ui_->contactList, partInt_);

  aboutWidget_ = new Ui::AboutWidget;
  aboutWidget_->setupUi(&about_);

  // I don't know why this is required.
  qRegisterMetaType<QVector<int> >("QVector<int>");

  QObject::connect(&settingsView_, &Settings::updateCallSettings,
                   this,           &CallWindow::updateCallSettings);

  QObject::connect(&settingsView_, &Settings::updateVideoSettings,
                   this,           &CallWindow::updateVideoSettings);

  QObject::connect(&settingsView_, &Settings::updateAudioSettings,
                   this,           &CallWindow::updateAudioSettings);

  QObject::connect(ui_->mic, &QPushButton::clicked,
                   this, &CallWindow::micButton);

  QObject::connect(ui_->camera, &QPushButton::clicked,
                   this, &CallWindow::cameraButton);

  QObject::connect(ui_->EndCallButton, SIGNAL(clicked()),
                   this, SIGNAL(endCall()));

  QObject::connect(ui_->actionClose, SIGNAL(triggered()),
                   this, SIGNAL(closed()));

  QObject::connect(ui_->address, &QLineEdit::textChanged,
                   this, &CallWindow::changedSIPText);

  QObject::connect(ui_->username, &QLineEdit::textChanged,
                   this, &CallWindow::changedSIPText);

  QObject::connect(ui_->screen_share, &QPushButton::clicked,
                   this, &CallWindow::screensShareButton);


  QMainWindow::show();

  QObject::connect(&conference_, &ConferenceView::acceptCall, this, &CallWindow::callAccepted);
  QObject::connect(&conference_, &ConferenceView::rejectCall, this, &CallWindow::callRejected);
  QObject::connect(&conference_, &ConferenceView::cancelCall, this, &CallWindow::callCancelled);

  conference_.init(ui_->participantLayout, ui_->participants);

  initButton(QDir::currentPath() + "/icons/add_contact.svg", QSize(60,60), QSize(35,35), ui_->addContact);
  initButton(QDir::currentPath() + "/icons/settings.svg", QSize(60,60), QSize(35,35), ui_->settings_button);
  initButton(QDir::currentPath() + "/icons/video_on.svg", QSize(60,60), QSize(35,35), ui_->camera);
  initButton(QDir::currentPath() + "/icons/mic_on.svg", QSize(60,60), QSize(35,35), ui_->mic);
  initButton(QDir::currentPath() + "/icons/end_call.svg", QSize(60,60), QSize(35,35), ui_->EndCallButton);
  initButton(QDir::currentPath() + "/icons/screen_share.svg", QSize(60,60), QSize(35,35), ui_->screen_share);

  ui_->buttonContainer->layout()->setAlignment(ui_->endcallHolder, Qt::AlignBottom);
  ui_->buttonContainer->layout()->setAlignment(ui_->settings_button, Qt::AlignBottom);
  ui_->buttonContainer->layout()->setAlignment(ui_->mic, Qt::AlignBottom);
  ui_->buttonContainer->layout()->setAlignment(ui_->camera, Qt::AlignBottom);
  ui_->buttonContainer->layout()->setAlignment(ui_->SelfView, Qt::AlignBottom);
  ui_->buttonContainer->layout()->setAlignment(ui_->screen_share, Qt::AlignBottom);

  ui_->contactListContainer->layout()->setAlignment(ui_->addContact, Qt::AlignHCenter);

  ui_->EndCallButton->hide();

  settingsView_.init();
  settingsView_.setScreenShareState(false);

  // set button icons to correct states
  setMicState(settingEnabled(SettingsKey::micStatus));
  setCameraState(settingEnabled(SettingsKey::cameraStatus));
}


void CallWindow::initButton(QString iconPath, QSize size, QSize iconSize,
                            QPushButton* button)
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
  else
  {
    qDebug() << "WARNING," << metaObject()->className() << ": Could not find icon:" << iconPath;
  }
}

StatisticsInterface* CallWindow::createStatsWindow()
{
  printNormal(this,"Initiating, CallWindow: Creating statistics window");
  statsWindow_ = new StatisticsWindow(this);

  // Stats GUI updates are handled solely by timer
  timer_->setInterval(200);
  timer_->setSingleShot(false);
  timer_->start();

  connect(timer_, SIGNAL(timeout()), statsWindow_, SLOT(update()));
  return statsWindow_;
}

void CallWindow::on_addContact_clicked()
{
  printNormal(this, "Clicked");
  QString serverAddress = settingString(SettingsKey::sipServerAddress);
  ui_->address->setText(serverAddress);
  ui_->username->setText("username");

  changedSIPText(serverAddress);
}

void CallWindow::addContact()
{
  // TODO: support dns
  QRegularExpression re_ip ("\\b((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)(\\.|$)){4}\\b");
  QRegularExpressionMatch ip_match = re_ip.match(ui_->address->text());

  // has the user inputted correct information
  if (!ip_match.hasMatch())
  {
    ui_->addressLabel->setText("Please set an IP address!");
    ui_->addressLabel->setStyleSheet("QLabel { color : red; }");
  }
  else if (ui_->username->text() == "")
  {
    ui_->usernameLabel->setText("Please set username!");
    ui_->usernameLabel->setStyleSheet("QLabel { color : red; }");
  }
  else
  {
    contacts_.addContact(partInt_, ui_->peerName->text(), ui_->username->text(), ui_->address->text());

    // Clean the add menu after successful add
    ui_->peerName->clear();
    ui_->username->clear();
    ui_->address->clear();
    ui_->addressLabel->setText("Address");
    ui_->addressLabel->setStyleSheet("QLabel { color : black; }");

    ui_->usernameLabel->setText("Username");
    ui_->usernameLabel->setStyleSheet("QLabel { color : black; }");
    ui_->Add_contact_widget->hide();

    ui_->addContact->show();
  }
}


void CallWindow::displayOutgoingCall(uint32_t sessionID, QString name)
{
  contacts_.turnAllItemsToPlus();
  conference_.callingTo(sessionID, name); // TODO get name from contact list
}

void CallWindow::displayIncomingCall(uint32_t sessionID, QString caller)
{
  conference_.incomingCall(sessionID, caller);
}

void CallWindow::displayRinging(uint32_t sessionID)
{
  conference_.ringing(sessionID);
}

void CallWindow::openStatistics()
{
  if(statsWindow_)
  {
    statsWindow_->show();
  }
  else
  {
    qDebug() << "WARNING," << metaObject()->className()
             << ": No stats window class initiated";
  }
}

void CallWindow::closeEvent(QCloseEvent *event)
{
  emit closed();
  statsWindow_->hide();
  statsWindow_->finished(0);

  QMainWindow::closeEvent(event);
}

void CallWindow::addVideoStream(uint32_t sessionID)
{
  ui_->EndCallButton->setEnabled(true);
  ui_->EndCallButton->show();
  conference_.addVideoStream(sessionID, viewFactory_);
}

void CallWindow::setMicState(bool on)
{
  if(on)
  {
    initButton(QDir::currentPath() + "/icons/mic_on.svg", QSize(60,60), QSize(35,35), ui_->mic);
    //ui_->mic->setText("Mic off");
  }
  else
  {
    initButton(QDir::currentPath() + "/icons/mic_off.svg", QSize(60,60), QSize(35,35), ui_->mic);
    //ui_->mic->setText("Mic on");
  }
}

void CallWindow::setCameraState(bool on)
{
  if(on)
  {
    initButton(QDir::currentPath() + "/icons/video_on.svg",
               QSize(60,60), QSize(35,35), ui_->camera);
  }
  else
  {
    initButton(QDir::currentPath() + "/icons/video_off.svg",
               QSize(60,60), QSize(35,35), ui_->camera);
  }
}


void CallWindow::removeParticipant(uint32_t sessionID)
{
  Q_ASSERT(sessionID != 0);

  if (sessionID == 0)
  {
    //printDebug();
    return;
  }

  if(!conference_.removeCaller(sessionID))
  {
    ui_->EndCallButton->setEnabled(false);
    ui_->EndCallButton->hide();
    contacts_.setAccessibleAll();
  }

  contacts_.setAccessible(sessionID);
  viewFactory_->clearWidgets(sessionID);
}


void CallWindow::removeWithMessage(uint32_t sessionID, QString message, bool temporaryMessage)
{
  removeParticipant(sessionID);
  conference_.attachMessageWidget(message, temporaryMessage);
}


void CallWindow::on_settings_button_clicked()
{
  settingsView_.show();
}


void CallWindow::screensShareButton()
{
  printNormal(this, "Changing state of screen share");

  // we change the state of screensharestatus setting here
  settingsView_.setScreenShareState(!settingEnabled(SettingsKey::screenShareStatus));

  emit updateVideoSettings();
}


void CallWindow::micButton(bool checked)
{
  Q_UNUSED(checked);

  bool currentState = settingEnabled(SettingsKey::micStatus);

  setMicState(!currentState);
  settingsView_.setMicState(!currentState);
  emit updateAudioSettings();
}


void CallWindow::cameraButton(bool checked)
{
  Q_UNUSED(checked);

  bool currentState = settingEnabled(SettingsKey::cameraStatus);

  setCameraState(!currentState);
  settingsView_.setCameraState(!currentState);
  emit updateVideoSettings();
}


void CallWindow::on_about_clicked()
{
  about_.show();
}


void CallWindow::changedSIPText(const QString &text)
{
  Q_UNUSED(text);
  ui_->sipAddress->setText("sip:" + ui_->username->text() + "@" + ui_->address->text());
}


void CallWindow::showICEFailedMessage()
{
  mesg_.showError("Error: ICE Failed",
                  "ICE did not succeed to find a connection. "
                  "You may try again. If the issue persists, "
                  "please report the issue to Github if you are connected to the internet "
                  "and describe your network setup.");
}


void CallWindow::showCryptoMissingMessage()
{
  mesg_.showWarning("Warning: Encryption not possible",
                    "Crypto++ has not been included in both Kvazzup and uvgRTP.");
}


void CallWindow::showZRTPFailedMessage(QString sessionID)
{
  mesg_.showError("Error: ZRTP handshake has failed for session " + sessionID,
                  "Could not exchange encryption keys.");
}
