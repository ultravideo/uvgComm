#include "callwindow.h"

#include "global.h"
#include "videoviewfactory.h"

#include "common.h"
#include "settingskeys.h"
#include "logger.h"

#include "ui_callwindow.h"

#include <QCloseEvent>
#include <QTimer>
#include <QMetaType>
#include <QDir>


CallWindow::CallWindow(QWidget *parent):
  QMainWindow(parent),
  ui_(new Ui::CallWindow),
  conference_(this),
  viewFactory_(std::shared_ptr<VideoviewFactory>(new VideoviewFactory())),
  partInt_(nullptr)
{
  ui_->setupUi(this);
}


CallWindow::~CallWindow()
{
  delete ui_;
}


QList<VideoInterface*> CallWindow::getSelfVideos () const
{
  return viewFactory_->getSelfVideos();
}


void CallWindow::init(ParticipantInterface *partInt, VideoWidget* settingsVideo)
{ 
  partInt_ = partInt;

  viewFactory_->addSelfview(ui_->SelfView);
  viewFactory_->addSelfview(settingsVideo);

  ui_->Add_contact_widget->setVisible(false);

  setWindowTitle("Kvazzup");

  contacts_.initializeList(ui_->contactList, partInt_);

  // I don't know why this is required.
  qRegisterMetaType<QVector<int> >("QVector<int>");

  QObject::connect(ui_->mic, &QPushButton::clicked,
                   this, &CallWindow::micButton);

  QObject::connect(ui_->camera, &QPushButton::clicked,
                   this, &CallWindow::cameraButton);

  QObject::connect(ui_->screen_share, &QPushButton::clicked,
                   this, &CallWindow::screensShareButton);

  QObject::connect(ui_->settings_button, &QPushButton::clicked,
                   this, &CallWindow::openSettings);

  QObject::connect(ui_->EndCallButton, &QPushButton::clicked,
                   this, &CallWindow::endCall);

  QObject::connect(ui_->address, &QLineEdit::textChanged,
                   this, &CallWindow::changedSIPText);

  QObject::connect(ui_->username, &QLineEdit::textChanged,
                   this, &CallWindow::changedSIPText);

  QObject::connect(&removeLayoutTimer_, &QTimer::timeout,
                  this,                 &CallWindow::expireLayouts);

  QMainWindow::show();

  QObject::connect(&conference_, &ConferenceView::acceptCall,       this, &CallWindow::layoutAccepts);
  QObject::connect(&conference_, &ConferenceView::rejectCall,       this, &CallWindow::layoutRejects);
  QObject::connect(&conference_, &ConferenceView::cancelCall,       this, &CallWindow::layoutCancels);
  QObject::connect(&conference_, &ConferenceView::messageConfirmed, this, &CallWindow::removeLayout);

  conference_.init(ui_->participantLayout, ui_->participants);

  initButton(QDir::currentPath() + "/icons/add_contact.svg",  QSize(60,60), QSize(35,35), ui_->addContact);
  initButton(QDir::currentPath() + "/icons/settings.svg",     QSize(60,60), QSize(35,35), ui_->settings_button);
  initButton(QDir::currentPath() + "/icons/video_on.svg",     QSize(60,60), QSize(35,35), ui_->camera);
  initButton(QDir::currentPath() + "/icons/mic_on.svg",       QSize(60,60), QSize(35,35), ui_->mic);
  initButton(QDir::currentPath() + "/icons/end_call.svg",     QSize(60,60), QSize(35,35), ui_->EndCallButton);
  initButton(QDir::currentPath() + "/icons/screen_share.svg", QSize(60,60), QSize(35,35), ui_->screen_share);

  ui_->buttonContainer->layout()->setAlignment(ui_->end_call_holder, Qt::AlignBottom);
  ui_->buttonContainer->layout()->setAlignment(ui_->settings_button, Qt::AlignBottom);
  ui_->buttonContainer->layout()->setAlignment(ui_->mic,             Qt::AlignBottom);
  ui_->buttonContainer->layout()->setAlignment(ui_->camera,          Qt::AlignBottom);
  ui_->buttonContainer->layout()->setAlignment(ui_->SelfView,        Qt::AlignBottom);
  ui_->buttonContainer->layout()->setAlignment(ui_->screen_share,    Qt::AlignBottom);

  QObject::connect(ui_->actionAbout,      &QAction::triggered, this, &CallWindow::openAbout);
  QObject::connect(ui_->actionSettings,   &QAction::triggered, this, &CallWindow::openSettings);
  QObject::connect(ui_->actionStatistics, &QAction::triggered, this, &CallWindow::openStatistics);
  QObject::connect(ui_->actionClose,      &QAction::triggered, this, &CallWindow::close);


  ui_->contactListContainer->layout()->setAlignment(ui_->addContact, Qt::AlignHCenter);

  ui_->EndCallButton->hide();

  // set button icons to correct states
  setMicState(settingEnabled(SettingsKey::micStatus));
  setCameraState(settingEnabled(SettingsKey::cameraStatus));
  setScreenShareState(settingEnabled(SettingsKey::screenShareStatus));
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
    Logger::getLogger()->printWarning(this, "Could not find icon", "Path", iconPath);
  }
}


void CallWindow::addContactButton()
{
  Logger::getLogger()->printNormal(this, "Clicked");
  QString serverAddress = settingString(SettingsKey::sipServerAddress);
  ui_->address->setText(serverAddress);
  ui_->username->setText("username");

  changedSIPText(serverAddress);
}


void CallWindow::addContact()
{
  // TODO: support DNS
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
  checkID(sessionID);

  for (auto& layoutID : layoutIDs_.at(sessionID))
  {
    conference_.attachOutgoingCallWidget(layoutID, name);  // TODO get name from contact list
  }
}


void CallWindow::displayRinging(uint32_t sessionID)
{
  checkID(sessionID);

  for (auto& layoutID : layoutIDs_.at(sessionID))
  {
    conference_.attachRingingWidget(layoutID);
  }
}


void CallWindow::displayIncomingCall(uint32_t sessionID, QString caller)
{
  checkID(sessionID);

  for (auto& layoutID : layoutIDs_.at(sessionID))
  {
    conference_.attachIncomingCallWidget(layoutID, caller);
  }
}


void CallWindow::closeEvent(QCloseEvent *event)
{
  emit closed();
  QMainWindow::closeEvent(event);
}


VideoInterface *CallWindow::callStarted(uint32_t sessionID,
                                        QList<SDPMediaParticipant>& medias)
{
  ui_->EndCallButton->setEnabled(true);
  ui_->EndCallButton->show();

  checkID(sessionID);

  // create the needed amount of layoutIDs
  for (unsigned int i = layoutIDs_[sessionID].size(); i < medias.size(); ++i)
  {
    layoutIDs_[sessionID].push_back(conference_.createLayoutID());
  }

  // change the contents of layouts to medias
  for (unsigned int i = 0; i < medias.size(); ++i)
  {
    uint32_t layoutID = layoutIDs_[sessionID].at(i);
    if (medias.at(i).videoEnabled)
    {
      conference_.attachVideoWidget(layoutID, viewFactory_->getView(layoutID));
    }
    else
    {
      conference_.attachAvatarWidget(layoutID, medias.at(i).name);
    }

    viewFactory_->getVideo(layoutID)->drawMicOffIcon(!medias.at(i).audioEnabled);
  }

  return viewFactory_->getVideo(sessionID); // TODO: LayoutID
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

  if (on)
  {
    ui_->SelfView->show();
  }
  else
  {
    ui_->SelfView->hide();
  }
}


void CallWindow::setScreenShareState(bool on)
{
  Q_UNUSED(on)
  // TODO: Change screen share icon
}


void CallWindow::removeParticipant(uint32_t sessionID)
{
  Q_ASSERT(sessionID != 0);

  if (sessionID == 0)
  {
    //Logger::getLogger()->printDebug();
    return;
  }

  checkID(sessionID);
  for (auto& layoutID : layoutIDs_.at(sessionID))
  {
    conference_.removeWidget(layoutID);
  }

  layoutIDs_.erase(sessionID);
  contacts_.setAccessible(sessionID);
  viewFactory_->clearWidgets(sessionID);
}


void CallWindow::removeWithMessage(uint32_t sessionID, QString message,
                                   bool temporaryMessage)
{
  if (layoutExists(sessionID))
  {
    for (auto& layoutID : layoutIDs_.at(sessionID))
    {
      conference_.attachMessageWidget(layoutID, message, !temporaryMessage);

      if (temporaryMessage)
      {
        expiringLayouts_.push_back(layoutID);
      }
    }
  }

  if (temporaryMessage)
  {
    int timeout = 3000;

    removeLayoutTimer_.setSingleShot(true);
    removeLayoutTimer_.start(timeout);

  }
}


void CallWindow::screensShareButton()
{
  Logger::getLogger()->printNormal(this, "Changing state of screen share");

  // TODO: Show a preview of each screen/window

  // we change the state of screensharestatus setting here
  emit videoSourceChanged(settingEnabled(SettingsKey::cameraStatus),
                          !settingEnabled(SettingsKey::screenShareStatus));
}


void CallWindow::micButton(bool checked)
{
  Q_UNUSED(checked);

  bool currentState = !settingEnabled(SettingsKey::micStatus);

  setMicState(currentState);
  emit audioSourceChanged(currentState);
}


void CallWindow::cameraButton(bool checked)
{
  Q_UNUSED(checked);

  bool currentState = !settingEnabled(SettingsKey::cameraStatus);

  setCameraState(currentState);
  emit videoSourceChanged(currentState,
                          settingEnabled(SettingsKey::screenShareStatus));
}


void CallWindow::layoutAccepts(uint32_t layoutID)
{
  emit callAccepted(layoutIDToSessionID(layoutID));
}


void CallWindow::layoutRejects(uint32_t layoutID)
{
  emit callRejected(layoutIDToSessionID(layoutID));
}


void CallWindow::layoutCancels(uint32_t layoutID)
{
  emit callCancelled(layoutIDToSessionID(layoutID));
}


void CallWindow::changedSIPText(const QString &text)
{
  Q_UNUSED(text);
  ui_->sipAddress->setText("sip:" + ui_->username->text() + "@" + ui_->address->text());
}


void CallWindow::restoreCallUI()
{
  ui_->EndCallButton->setEnabled(false);
  ui_->EndCallButton->hide();
  contacts_.setAccessibleAll();
}


void CallWindow::checkID(uint32_t sessionID)
{
  if (!layoutExists(sessionID))
  {
    layoutIDs_[sessionID].push_back(conference_.createLayoutID());
  }
}


bool CallWindow::layoutExists(uint32_t sessionID)
{
  return layoutIDs_.find(sessionID) != layoutIDs_.end() && !layoutIDs_[sessionID].empty();
}


void CallWindow::expireLayouts()
{
  for (auto& layoutID : expiringLayouts_)
  {
    removeLayout(layoutID);
  }

  expiringLayouts_.clear();
}


void CallWindow::removeLayout(uint32_t layoutID)
{
  conference_.removeWidget(layoutID);

  // remove the layout id also from call window tracking
  for (auto& sessionID : layoutIDs_)
  {
    for (unsigned int i = 0; i < sessionID.second.size(); ++i)
    {
      if (sessionID.second.at(i) == layoutID)
      {
        sessionID.second.erase(sessionID.second.begin() + i);
        break;
      }
    }
  }

  cleanUp();
}


uint32_t CallWindow::layoutIDToSessionID(uint32_t layoutID)
{
  for (auto& sessionID : layoutIDs_)
  {
    for (auto& ownedLayoutID : sessionID.second)
    {
      if (ownedLayoutID == layoutID)
      {
        return sessionID.first;
      }
    }
  }

  Logger::getLogger()->printProgramError(this, "Could not found mapping from layoutID to sessionID!");
  return 0;
}


void CallWindow::cleanUp()
{
  std::vector<uint32_t> toRemove;

  for (auto& sessionID : layoutIDs_)
  {
    if (sessionID.second.empty())
    {
      toRemove.push_back(sessionID.first);
    }
  }

  for (auto& sessionID : toRemove)
  {
    layoutIDs_.erase(sessionID);
  }

  if (layoutIDs_.empty())
  {
    restoreCallUI();
  }
}

