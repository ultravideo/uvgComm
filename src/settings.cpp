#include "settings.h"
#include "ui_basicsettings.h"
#include "ui_advancedsettings.h"

#include <QDebug>


const int SETTINGCOUNT = 10;

Settings::Settings(QWidget *parent) :
  QObject(parent),
  basicUI_(new Ui::BasicSettings),
  advancedUI_(new Ui::AdvancedSettings)
{
  basicUI_->setupUi(&basicParent_);
  advancedUI_->setupUi(&advancedParent_);
  restoreSettings(); // initializes the GUI with values

  QObject::connect(basicUI_->ok, SIGNAL(clicked()), this, SLOT(on_ok_clicked()));
  QObject::connect(basicUI_->cancel, SIGNAL(clicked()), this, SLOT(on_cancel_clicked()));
}

Settings::~Settings()
{
  // I think the UI:s are destroyed when parents are destroyed
  //delete basicUI_;
  //delete advancedUI_;
}

void Settings::on_ok_clicked()
{
  qDebug() << "Saving settings";
  saveSettings();
  emit settingsChanged();
  basicParent_.hide();
}

void Settings::on_cancel_clicked()
{
  qDebug() << "Getting settings from system";
  restoreSettings();
  basicParent_.hide();
}

// temparily records the settings
void Settings::saveSettings()
{
  // Local settings
  QSettings settings;
  if(basicUI_->name_edit->text() != "")
    settings.setValue("local/Name",         basicUI_->name_edit->text());
  if(basicUI_->username_edit->text() != "")
    settings.setValue("local/Username",     basicUI_->username_edit->text());

  // Video settings
  settings.setValue("video/Preset",       advancedUI_->preset->currentText());

  if(advancedUI_->scaled_height->text() != "")
    settings.setValue("video/ScaledHeight", advancedUI_->scaled_height->text());
  if(advancedUI_->scaled_width->text() != "")
    settings.setValue("video/ScaledWidth",  advancedUI_->scaled_width->text());
  if(advancedUI_->threads->text() != "")
    settings.setValue("video/Threads",      advancedUI_->threads->text());

  settings.setValue("video/QP",             QString::number(advancedUI_->qp->value()));

  if(advancedUI_->wpp->isChecked())
    settings.setValue("video/WPP",          "1");
  else
    settings.setValue("video/WPP",          "0");

  if(advancedUI_->vps->text() != "")
    settings.setValue("video/VPS",          advancedUI_->vps->text());
  if(advancedUI_->intra->text() != "")
    settings.setValue("video/Intra",        advancedUI_->intra->text());

  //settings.sync(); // TODO is this needed?
}

// restores temporarily recorded settings
void Settings::restoreSettings()
{
  QSettings settings;
  bool missingSettings = false;

  QStringList list = settings.allKeys();

  for(auto key : list)
  {
    qDebug() << "Found settings key:" << key << "value:" << settings.value(key).toString();
    if(!missingSettings && settings.value(key).isNull())
      missingSettings = true;
  }

  if(list.size() < SETTINGCOUNT) // Remember to update this value
  {
    qDebug() << "Settings found:" << list.size() << "Expected:" << SETTINGCOUNT;
    missingSettings = true;
  }

  //get values from QSettings
  if(!missingSettings)
  {
    qDebug() << "Restoring previous settings";
    basicUI_->name_edit->setText      (settings.value("local/Name").toString());
    basicUI_->username_edit->setText  (settings.value("local/Username").toString());

    int index = advancedUI_->preset->findText(settings.value("video/Preset").toString());
    if(index != -1)
      advancedUI_->preset->setCurrentIndex(index);
    advancedUI_->scaled_height->setText  (settings.value("video/ScaledHeight").toString());
    advancedUI_->scaled_width->setText   (settings.value("video/ScaledWidth").toString());
    advancedUI_->threads->setText        (settings.value("video/Threads").toString());
    advancedUI_->qp->setValue            (settings.value("video/QP").toInt());

    if(settings.value("video/WPP").toString() == "1")
      advancedUI_->wpp->setChecked(true);
    else if(settings.value("video/WPP").toString() == "0")
      advancedUI_->wpp->setChecked(false);
    advancedUI_->vps->setText            (settings.value("video/VPS").toString());
    advancedUI_->intra->setText          (settings.value("video/Intra").toString());
  }
  else
  {
    // Some settings have not been initialized (due to new settings). Use defaults in GUI
    qDebug() << "Resettings settings to defaults";
    saveSettings();
  }
}

void Settings::showBasicSettings()
{
  basicParent_.show();
}

void Settings::showAdvancedSettings()
{
  advancedParent_.show();
}
