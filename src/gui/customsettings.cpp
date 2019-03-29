#include "customsettings.h"

#include "ui_advancedSettings.h"

#include <video/camerainfo.h>

#include <QTableWidgetItem>
#include <QDateTime>
#include <QMenu>

#include <QDebug>


const QStringList neededSettings = {"video/DeviceID",
                                    "video/Device",
                                    "video/ResolutionWidth",
                                    "video/ResolutionHeight",
                                    "video/WPP",
                                    "video/InputFormat",
                                    "video/Slices",
                                    "video/kvzThreads",
                                    "video/yuvThreads",
                                    "video/OPENHEVC_Threads",
                                    "video/FramerateID"};



CustomSettings::CustomSettings(QWidget* parent,
                               std::shared_ptr<CameraInfo> info)
  :
  QDialog(parent),
  currentDevice_(0),
  advancedUI_(new Ui::AdvancedSettings),
  cam_(info),
  settings_("kvazzup.ini", QSettings::IniFormat)
{
  advancedUI_->setupUi(this);

  // the buttons are named so that the slots are called automatically
  QObject::connect(advancedUI_->format_box, SIGNAL(activated(QString)), this, SLOT(initializeResolutions(QString)));
  QObject::connect(advancedUI_->resolution, SIGNAL(activated(QString)), this, SLOT(initializeFramerates(QString)));
}


void CustomSettings::init(int deviceID)
{
  advancedUI_->blockedUsers->setColumnCount(2);
  advancedUI_->blockedUsers->setHorizontalHeaderItem(0, new QTableWidgetItem(QString("Username")));
  advancedUI_->blockedUsers->setHorizontalHeaderItem(1, new QTableWidgetItem(QString("Date")));

  advancedUI_->blockedUsers->setColumnWidth(0, 240);
  advancedUI_->blockedUsers->setColumnWidth(1, 180);
  advancedUI_->blockedUsers->setEditTriggers(QAbstractItemView::NoEditTriggers); // disallow editing of fields.

  currentDevice_ = deviceID;
  advancedUI_->blockedUsers->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(advancedUI_->blockedUsers, &QWidget::customContextMenuRequested,
          this, &CustomSettings::showBlocklistContextMenu);

  restoreAdvancedSettings();
}


void CustomSettings::changedDevice(uint16_t deviceIndex)
{
  currentDevice_ = deviceIndex;
  initializeFormat();
  saveCameraCapabilities(deviceIndex); // record the new camerasettings.
}


void CustomSettings::resetSettings(int deviceID)
{
  qDebug() << "Settings," << metaObject()->className()
           << "Resetting custom settings from UI";
  currentDevice_ = deviceID;
  initializeFormat();
  // TODO: should we do something to blocked list in settings?
  saveAdvancedSettings();
}


void CustomSettings::showBlocklistContextMenu(const QPoint& pos)
{
  qDebug() << "Settings," << metaObject()->className() << ": Showing context menu for blocked users.";

  // Handle global position
  QPoint globalPos = advancedUI_->blockedUsers->mapToGlobal(pos);

  // Create menu and insert some actions
  QMenu myMenu;
  myMenu.addAction("Delete", this, SLOT(deleteBlocklistItem()));

  // Show context menu at handling position
  myMenu.exec(globalPos);
}


void CustomSettings::deleteBlocklistItem()
{
  qDebug() << "Settings," << metaObject()->className() << ": deleting row:" << advancedUI_->blockedUsers->currentRow();
  advancedUI_->blockedUsers->removeRow(advancedUI_->blockedUsers->currentRow());
}


void CustomSettings::on_custom_ok_clicked()
{
  qDebug() << "Settings," << metaObject()->className() << ": Saving advanced settings";
  saveAdvancedSettings();
  emit customSettingsChanged();
  emit hidden();
  hide();
}


void CustomSettings::on_custom_cancel_clicked()
{
  qDebug() << "Settings," << metaObject()->className() << ": Cancelled modifying custom settings. Getting settings from system";
  restoreAdvancedSettings();
  hide();
  emit hidden();
}


void CustomSettings::on_addUserBlock_clicked()
{
  if(advancedUI_->blockUser->text() != "")
  {
    for(int i = 0; i < advancedUI_->blockedUsers->rowCount(); ++i)
    {
      if(advancedUI_->blockedUsers->item(i,0)->text() == advancedUI_->blockUser->text())
      {
        qDebug() << "Settings," << metaObject()->className() << ": Name already exists at row:" << i;
        advancedUI_->BlockUsernameLabel->setText("Name already blocked");
        return;
      }
    }

    qDebug() << "Settings," << metaObject()->className() << ":Blocking an user";

    addUsernameToList(advancedUI_->blockUser->text(), QDateTime::currentDateTime().toString("yyyy-mm-dd hh:mm"));

    advancedUI_->BlockUsernameLabel->setText("Block contacts from username:");
    advancedUI_->blockUser->setText("");
  }
  else
  {
    advancedUI_->BlockUsernameLabel->setText("Write username below:");
  }
}


void CustomSettings::serverStatusChange(QString status)
{
  advancedUI_->StatusLabel->setText(status);

  if(status == "Connected")
  {
    advancedUI_->connectButton->setText("Disconnect");
  }
}


void CustomSettings::saveAdvancedSettings()
{
  qDebug() << "Settings," << metaObject()->className() << ": Saving advanced Settings";

  // Video settings
  saveTextValue("video/kvzThreads",            advancedUI_->kvz_threads->text());
  saveTextValue("video/OPENHEVC_threads",      advancedUI_->openhevc_threads->text());
  saveTextValue("video/yuvThreads",            advancedUI_->yuv_threads->text());
  saveTextValue("video/VPS",                   advancedUI_->vps->text());
  saveTextValue("video/Intra",                 advancedUI_->intra->text());
  saveCheckBox("video/WPP",                    advancedUI_->wpp);
  saveCheckBox("video/Slices",                 advancedUI_->slices);

  settings_.setValue("video/QP",               QString::number(advancedUI_->qp->value()));
  settings_.setValue("video/Preset",           advancedUI_->preset->currentText());

  saveCheckBox("video/opengl",                 advancedUI_->opengl);
  saveCheckBox("video/flipViews",              advancedUI_->flip);
  saveCheckBox("video/liveCopying",            advancedUI_->live555Copy);

  settings_.setValue("audio/Channels",         QString::number(advancedUI_->channels->value()));

  saveCameraCapabilities(settings_.value("video/DeviceID").toInt());

  writeBlocklistToSettings();

  // sip settings.
  saveTextValue("sip/ServerAddress", advancedUI_->serverAddress->text());
  saveCheckBox("sip/AutoConnect", advancedUI_->autoConnect);

  //settings.sync(); // TODO is this needed?
}

void CustomSettings::saveCameraCapabilities(int deviceIndex)
{
  qDebug() << "Settings," << metaObject()->className() << ": Recording capability settings for deviceIndex:" << deviceIndex;

  int formatIndex = advancedUI_->format_box->currentIndex();
  int resolutionIndex = advancedUI_->resolution->currentIndex();

  qDebug() << "Settings," << metaObject()->className() << ": Boxes in following positions: Format:" << formatIndex << "Resolution:" << resolutionIndex;
  if(formatIndex == -1)
  {
    formatIndex = 0;
    resolutionIndex = 0;
  }

  if(resolutionIndex == -1)
  {
    resolutionIndex = 0;
  }

  QString format = cam_->getFormat(currentDevice_, formatIndex);
  QSize res = cam_->getResolution(currentDevice_, formatIndex, resolutionIndex);

  // since kvazaar requires resolution to be divisible by eight
  // TODO: Use QSize to record resolution
  settings_.setValue("video/ResolutionWidth",      res.width() - res.width()%8);
  settings_.setValue("video/ResolutionHeight",     res.height() - res.height()%8);
  settings_.setValue("video/ResolutionID",         resolutionIndex);
  settings_.setValue("video/FramerateID",          advancedUI_->framerate_box->currentIndex());
  settings_.setValue("video/InputFormat",          format);

  qDebug() << "Settings," << metaObject()->className() << ": Recorded the following video settings: Resolution:"
           << res.width() - res.width()%8 << "x" << res.height() - res.height()%8
           << "resolution index:" << resolutionIndex << "format" << format;
}

void CustomSettings::restoreAdvancedSettings()
{
  initializeFormat();
  initializeBlocklist();

  bool validSettings = checkMissingValues();
  if(validSettings && checkVideoSettings())
  {
    restoreFormat();
    restoreResolution();
    restoreFramerate();

    qDebug() << "Settings," << metaObject()->className() << ": Restoring previous Advanced settings from file:" << settings_.fileName();
    int index = advancedUI_->preset->findText(settings_.value("video/Preset").toString());
    if(index != -1)
      advancedUI_->preset->setCurrentIndex(index);
    advancedUI_->kvz_threads->setText        (settings_.value("video/kvzThreads").toString());
    advancedUI_->qp->setValue            (settings_.value("video/QP").toInt());
    advancedUI_->openhevc_threads->setText        (settings_.value("video/OPENHEVC_threads").toString());

    advancedUI_->yuv_threads->setText        (settings_.value("video/yuvThreads").toString());

    restoreCheckBox("video/WPP", advancedUI_->wpp);

    advancedUI_->vps->setText            (settings_.value("video/VPS").toString());
    advancedUI_->intra->setText          (settings_.value("video/Intra").toString());

    restoreCheckBox("video/Slices", advancedUI_->slices);

    advancedUI_->format_box->setCurrentText(settings_.value("video/InputFormat").toString());

    int resolutionID = settings_.value("video/ResolutionID").toInt();
    advancedUI_->resolution->setCurrentIndex(resolutionID);

    restoreCheckBox("video/opengl", advancedUI_->opengl);
    restoreCheckBox("video/flipViews", advancedUI_->flip);
    restoreCheckBox("video/liveCopying", advancedUI_->live555Copy);

  }
  else
  {
    resetSettings(currentDevice_);
  }

  if(validSettings && checkAudioSettings())
  {
    // TODO: implement audio settings.
  }
  else
  {
    // TODO: reset only audio settings.
    resetSettings(currentDevice_);
  }

  if(validSettings && checkSipSettings())
  {
    restoreCheckBox("sip/AutoConnect", advancedUI_->autoConnect);
    advancedUI_->serverAddress->setText        (settings_.value("sip/ServerAddress").toString());
  }
  else
  {
    resetSettings(currentDevice_);
  }

  // TODO: check audio settings
  advancedUI_->channels->setValue(settings_.value("audio/Channels").toInt());
}

void CustomSettings::restoreFormat()
{
  if(advancedUI_->format_box->count() > 0)
  {
    // initialize right format
    QString format = "";
    if(settings_.contains("video/InputFormat"))
    {
      format = settings_.value("video/InputFormat").toString();
      int formatIndex = advancedUI_->format_box->findText(format);
      qDebug() << "Settings," << metaObject()->className() << ": Trying to find format in camera:" << format << "Result index:" << formatIndex;

      if(formatIndex != -1)
      {
        advancedUI_->format_box->setCurrentIndex(formatIndex);
      }
      else {
        advancedUI_->format_box->setCurrentIndex(0);
      }

    }
    else
    {
      advancedUI_->format_box->setCurrentIndex(0);
    }

    initializeResolutions(advancedUI_->format_box->currentText());
  }
}

void CustomSettings::restoreResolution()
{
  if(advancedUI_->resolution->count() > 0)
  {
    int resolutionID = settings_.value("video/ResolutionID").toInt();

    if(resolutionID >= 0)
    {
      advancedUI_->resolution->setCurrentIndex(resolutionID);
    }
    else {
      advancedUI_->resolution->setCurrentIndex(0);
    }
  }
}


void CustomSettings::restoreFramerate()
{
  if(advancedUI_->framerate_box->count() > 0)
  {
    int framerateID = settings_.value("video/FramerateID").toInt();

    if(framerateID < advancedUI_->framerate_box->count())
    {
      advancedUI_->framerate_box->setCurrentIndex(framerateID);
    }
    else {
      advancedUI_->framerate_box->setCurrentIndex(advancedUI_->framerate_box->count() - 1);
    }
  }
}


void CustomSettings::initializeFormat()
{
  qDebug() << "Settings," << metaObject()->className() << "Initializing formats";
  QStringList formats;

  cam_->getVideoFormats(currentDevice_, formats);

  advancedUI_->format_box->clear();
  for(int i = 0; i < formats.size(); ++i)
  {
    advancedUI_->format_box->addItem(formats.at(i));
  }

  if(advancedUI_->format_box->count() > 0)
  {
    advancedUI_->format_box->setCurrentIndex(0);
    initializeResolutions(advancedUI_->format_box->currentText());
  }
}


void CustomSettings::initializeResolutions(QString format)
{
  qDebug() << "Settings," << metaObject()->className() << ": Initializing resolutions for format:" << format;
  advancedUI_->resolution->clear();
  QStringList resolutions;

  cam_->getFormatResolutions(currentDevice_, format, resolutions);

  if(!resolutions.empty())
  {
    for(int i = 0; i < resolutions.size(); ++i)
    {
      advancedUI_->resolution->addItem(resolutions.at(i));
    }
  }

  if(advancedUI_->resolution->count() > 0)
  {
    advancedUI_->resolution->setCurrentIndex(0);
    initializeFramerates(format, 0);
  }
}

void CustomSettings::initializeFramerates(QString format, int resolutionID)
{
  qDebug() << "Settings,"  << metaObject()->className() << ": Initializing resolutions for format:" << format;
  advancedUI_->framerate_box->clear();
  QStringList rates;

  cam_->getFramerates(currentDevice_, format, resolutionID, rates);

  if(!rates.empty())
  {
    for(int i = 0; i < rates.size(); ++i)
    {
      advancedUI_->framerate_box->addItem(rates.at(i));
    }
  }
}


void CustomSettings::initializeBlocklist()
{
  //list_->setContextMenuPolicy(Qt::CustomContextMenu);
  //connect(list, SIGNAL(customContextMenuRequested(QPoint)),
  //        this, SLOT(showContextMenu(QPoint)));

  advancedUI_->blockedUsers->clear();
  advancedUI_->blockedUsers->setRowCount(0);

  QSettings settings("blocklist.local", QSettings::IniFormat);

  int size = settings.beginReadArray("blocklist");
  qDebug() << "Settings,"  << metaObject()->className() << ": Reading blocklist with" << size << "usernames";
  for (int i = 0; i < size; ++i) {
    settings.setArrayIndex(i);
    QString username = settings.value("userName").toString();
    QString date = settings.value("date").toString();

    addUsernameToList(username, date);
  }
  settings.endArray();
}


void CustomSettings::writeBlocklistToSettings()
{
  qDebug() << "Settings," << metaObject()->className() << "Writing blocklist with" << advancedUI_->blockedUsers->rowCount()
           << "items to settings.";

  QSettings settings("blocklist.local", QSettings::IniFormat);

  settings.beginWriteArray("blocklist");
  for(int i = 0; i < advancedUI_->blockedUsers->rowCount(); ++i)
  {
    settings.setArrayIndex(i);
    settings.setValue("username", advancedUI_->blockedUsers->item(i,0)->text());
    settings.setValue("date", advancedUI_->blockedUsers->item(i,1)->text());
  }
  settings.endArray();
}


void CustomSettings::addUsernameToList(QString username, QString date)
{
  QTableWidgetItem* itemUser = new QTableWidgetItem(username);
  QTableWidgetItem* itemDate = new QTableWidgetItem(date);

  advancedUI_->blockedUsers->insertRow(advancedUI_->blockedUsers->rowCount());

  advancedUI_->blockedUsers->setItem(advancedUI_->blockedUsers->rowCount() - 1, 0, itemUser);
  advancedUI_->blockedUsers->setItem(advancedUI_->blockedUsers->rowCount() - 1, 1, itemDate);
}


void CustomSettings::restoreCheckBox(const QString settingValue, QCheckBox* box)
{
  if(settings_.value(settingValue).toString() == "1")
  {
    box->setChecked(true);
  }
  else if(settings_.value(settingValue).toString() == "0")
  {
    box->setChecked(false);
  }
  else
  {
    qDebug() << "Settings," << metaObject()->className()
             << ": Corrupted value for checkbox in settings file for:" << settingValue << "!!!";
  }
}

void CustomSettings::saveCheckBox(const QString settingValue, QCheckBox* box)
{
  if(box->isChecked())
  {
    settings_.setValue(settingValue,          "1");
  }
  else
  {
    settings_.setValue(settingValue,          "0");
  }
}

void CustomSettings::saveTextValue(const QString settingValue, const QString &text)
{
  if(text != "")
  {
    settings_.setValue(settingValue,  text);
  }
}

bool CustomSettings::checkMissingValues()
{
  QStringList list = settings_.allKeys();

  bool foundEverything = true;
  for(auto key : list)
  {
    if(settings_.value(key).isNull() || settings_.value(key) == "")
    {
      qDebug() << "Settings," << metaObject()->className() << ": MISSING SETTING FOR:" << key;
      foundEverything = false;
    }
  }
  return foundEverything;
}

bool CustomSettings::checkVideoSettings()
{
  bool everythingPresent = checkMissingValues();

  for(auto need : neededSettings)
  {
    if(!settings_.contains(need))
    {
      qDebug() << "Settings," << metaObject()->className()
               << "Missing setting for:" << need << "Resetting custom settings";
      everythingPresent = false;
    }
  }
  return everythingPresent;
}

bool CustomSettings::checkAudioSettings()
{
  return true;
}

bool CustomSettings::checkSipSettings()
{
  return settings_.contains("sip/ServerAddress")
      && settings_.contains("sip/AutoConnect");
}
