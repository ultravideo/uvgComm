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
#include <QHostAddress>


CallWindow::CallWindow(QWidget *parent):
  QMainWindow(parent),
  ui_(new Ui::CallWindow),
  conference_(this),
  partInt_(nullptr)
{
  ui_->setupUi(this);
}


CallWindow::~CallWindow()
{
  delete ui_;
}

VideoWidget* CallWindow::getSelfView() const
{
  return ui_->SelfView;
}


void CallWindow::init(ParticipantInterface *partInt)
{ 
  partInt_ = partInt;

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

  initButton(":/icons/add_contact.svg",  QSize(60,60), QSize(35,35), ui_->addContact);
  initButton(":/icons/settings.svg",     QSize(60,60), QSize(35,35), ui_->settings_button);
  initButton(":/icons/video_on.svg",     QSize(60,60), QSize(35,35), ui_->camera);
  initButton(":/icons/mic_on.svg",       QSize(60,60), QSize(35,35), ui_->mic);
  initButton(":/icons/end_call.svg",     QSize(60,60), QSize(35,35), ui_->EndCallButton);
  initButton(":/icons/screen_share.svg", QSize(60,60), QSize(35,35), ui_->screen_share);

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
  QHostAddress address = QHostAddress(ui_->address->text());

  // TODO: support DNS

  // has the user inputted correct information
  if (ui_->username->text() == "")
  {
    ui_->usernameLabel->setText("Please set username!");
    ui_->usernameLabel->setStyleSheet("QLabel { color : red; }");
  }
  else if (address.isNull())
  {
    ui_->addressLabel->setText("Please set a valid IP address!");
    ui_->addressLabel->setStyleSheet("QLabel { color : red; }");
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

  LayoutID id;
  if (getTempLayoutID(id, sessionID))
  {
    conference_.attachOutgoingCallWidget(id, name);
  }
}


void CallWindow::displayRinging(uint32_t sessionID)
{
  checkID(sessionID);

  LayoutID id;
  if (getTempLayoutID(id, sessionID))
  {
    conference_.attachRingingWidget(id);
  }
}


void CallWindow::displayIncomingCall(uint32_t sessionID, QString caller)
{
  checkID(sessionID);

  LayoutID id;
  if (getTempLayoutID(id, sessionID))
  {
    conference_.attachIncomingCallWidget(id, caller);
  }
}


void CallWindow::closeEvent(QCloseEvent *event)
{
  emit closed();
  QMainWindow::closeEvent(event);
}


void CallWindow::callStarted(std::shared_ptr<VideoviewFactory> viewFactory,
                             uint32_t sessionID, QStringList names,
                             const QList<std::pair<MediaID,MediaID>> &audioVideoIDs)
{
  // Allow user to end the call
  ui_->EndCallButton->setEnabled(true);
  ui_->EndCallButton->show();

  // if the number of required layouts has reduced
  if (layoutIDs_[sessionID].size() > audioVideoIDs.size())
  {
    // find out which ones have disappeared
    std::vector<LayoutID> expiredIDs;

    // remove expired layout slots
    for (auto& layout : layoutIDs_[sessionID])
    {
      bool expiredLayout = true; // assume the layout is not needed unless proven otherwise

      for (auto& mid : audioVideoIDs)
      {
          // this layout is still needed
          if (layout.mediaID == mid.second)
        {
          expiredLayout = false;
        }
      }

      if (expiredLayout)
      {
        expiredIDs.push_back(layout.layoutID);
      }
    }

    Logger::getLogger()->printNormal(this, "Removing " + QString::number(expiredIDs.size()) + " medias from layout");

    for (auto& expired : expiredIDs)
    {
      conference_.removeWidget(expired);
      for (unsigned int i = 0; i < layoutIDs_[sessionID].size(); ++i)
      {
        if (expired == layoutIDs_[sessionID][i].layoutID)
        {
          layoutIDs_[sessionID].erase(layoutIDs_[sessionID].begin() + i);
          break;
        }
      }
    }
  }
  else // add more medias if necessary and update current ones status
  {
    // add new medias
    for (auto& mid : audioVideoIDs)
    {
      bool newMedia = true; // assume this media is new one unless proven otherwise

      for (auto& layout : layoutIDs_[sessionID])
      {
        if (layout.mediaID == mid.second)
        {
          // we already have a layout location for this media
           newMedia = false;
        }
      }

      // if new, allocate a layout location for it
      if (newMedia)
      {
        LayoutID id;
        // can we use the temporary layout use for call messages previously
        if (getTempLayoutID(id, sessionID))
        {
           Logger::getLogger()->printNormal(this, "Using existing layout position to show media");

           // move layout from temporary to media layouts
           temporaryLayoutIDs_.erase(sessionID);
        }
        else // no temporary layout available, so we create a new one
        {
           Logger::getLogger()->printNormal(this, "Using a new layout position for media");
           id = conference_.createLayoutID();
        }

        layoutIDs_[sessionID].push_back({id, mid.second});
        viewFactory->createWidget(sessionID, id, mid.second);
      }
    }

    unsigned int layoutsUsed = audioVideoIDs.size();

    if (layoutIDs_[sessionID].size() < layoutsUsed)
    {
      Logger::getLogger()->printWarning(this, "Duplicate medias detected!");
      layoutsUsed = layoutIDs_[sessionID].size();
    }

    // change the contents of layouts to medias
    for (unsigned int i = 0; i < layoutsUsed; ++i) {
      uint32_t layoutID = layoutIDs_[sessionID].at(i).layoutID;

      if (audioVideoIDs.at(i).second.getReceive()) // video or avatar
      {
         conference_.attachVideoWidget(
           layoutID,
           viewFactory->getView(layoutIDs_[sessionID].at(i).mediaID));
      } else {
         conference_.attachAvatarWidget(layoutID, names.at(i));
      }

      // update audio icon status
      viewFactory->getVideo(layoutIDs_[sessionID].at(i).mediaID)
        ->drawMicOffIcon(!audioVideoIDs.at(i).first.getReceive());
    }
  }
}


void CallWindow::setMicState(bool on)
{
  if(on)
  {
    initButton(":/icons/mic_on.svg", QSize(60,60), QSize(35,35), ui_->mic);
    //ui_->mic->setText("Mic off");
  }
  else
  {
    initButton(":/icons/mic_off.svg", QSize(60,60), QSize(35,35), ui_->mic);
    //ui_->mic->setText("Mic on");
  }
}


void CallWindow::setCameraState(bool on)
{
  if(on)
  {
    initButton(":/icons/video_on.svg", QSize(60,60), QSize(35,35), ui_->camera);
  }
  else
  {
    initButton(":/icons/video_off.svg", QSize(60,60), QSize(35,35), ui_->camera);
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
    conference_.removeWidget(layoutID.layoutID);
  }

  layoutIDs_.erase(sessionID);

  LayoutID id;
  if (getTempLayoutID(id, sessionID))
  {
    conference_.removeWidget(id);
    temporaryLayoutIDs_.erase(sessionID);
  }

  contacts_.setAccessible(sessionID);
}


void CallWindow::removeWithMessage(uint32_t sessionID, QString message,
                                   bool temporaryMessage)
{

  if (layoutIDs_.find(sessionID) != layoutIDs_.end())
  {
    for (auto& layoutID : layoutIDs_.at(sessionID))
    {
      conference_.attachMessageWidget(layoutID.layoutID, message, !temporaryMessage);

      if (temporaryMessage)
      {
        expiringLayouts_.push_back(layoutID.layoutID);
      }
    }
  }

  LayoutID id;
  if (getTempLayoutID(id, sessionID))
  {
    conference_.attachMessageWidget(id, message, !temporaryMessage);
    if (temporaryMessage)
    {
      expiringLayouts_.push_back(id);
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


void CallWindow::layoutAccepts(LayoutID layoutID)
{
  emit callAccepted(layoutIDToSessionID(layoutID));
}


void CallWindow::layoutRejects(LayoutID layoutID)
{
  emit callRejected(layoutIDToSessionID(layoutID));
}


void CallWindow::layoutCancels(LayoutID layoutID)
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
    temporaryLayoutIDs_[sessionID] = conference_.createLayoutID();
  }
}


bool CallWindow::layoutExists(uint32_t sessionID)
{
  return temporaryLayoutIDs_.find(sessionID) != temporaryLayoutIDs_.end() ||
         (layoutIDs_.find(sessionID) != layoutIDs_.end() && !layoutIDs_[sessionID].empty());
}


void CallWindow::expireLayouts()
{
  for (auto& layoutID : expiringLayouts_)
  {
    removeLayout(layoutID);
  }

  expiringLayouts_.clear();
}


void CallWindow::removeLayout(LayoutID layoutID)
{
  conference_.removeWidget(layoutID);

  // remove the layout id also from call window tracking
  for (auto& sessionID : layoutIDs_)
  {
    for (unsigned int i = 0; i < sessionID.second.size(); ++i)
    {
      if (sessionID.second.at(i).layoutID == layoutID)
      {
        sessionID.second.erase(sessionID.second.begin() + i);
        break;
      }
    }
  }

  for (auto sessionID = temporaryLayoutIDs_.begin();
       sessionID != temporaryLayoutIDs_.end(); ++sessionID)
  {
    if (sessionID->second == layoutID)
    {
      temporaryLayoutIDs_.erase(sessionID);
      break;
    }
  }


  cleanUp();
}


uint32_t CallWindow::layoutIDToSessionID(LayoutID layoutID)
{
  for (auto& sessionID : layoutIDs_)
  {
    for (auto& ownedLayoutID : sessionID.second)
    {
      if (ownedLayoutID.layoutID == layoutID)
      {
        return sessionID.first;
      }
    }
  }

  for (auto& sessionID : temporaryLayoutIDs_)
  {
    if (sessionID.second == layoutID)
    {
      return sessionID.first;
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

  if (layoutIDs_.empty() && temporaryLayoutIDs_.empty())
  {
    restoreCallUI();
  }
}


bool CallWindow::getTempLayoutID(LayoutID& id, uint32_t sessionID)
{
  bool found = temporaryLayoutIDs_.find(sessionID) != temporaryLayoutIDs_.end();

  if (found)
  {
    id = temporaryLayoutIDs_[sessionID];
  }

  return found;
}
