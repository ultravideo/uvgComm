#include "settings.h"
#include "ui_settings.h"

#include <QDebug>


const int SETTINGCOUNT = 10;

Settings::Settings(QWidget *parent) :
  QDialog(parent),
  ui(new Ui::Settings)
{
  ui->setupUi(this);
  restoreSettings(); // initializes the GUI with values
}

Settings::~Settings()
{
  delete ui;
}

void Settings::on_ok_clicked()
{
  qDebug() << "Settings have been modified";
  saveSettings();
  emit settingsChanged();
  hide();
}

void Settings::on_cancel_clicked()
{
  qDebug() << "Getting settings from system";
  restoreSettings();
  hide();
}

// temparily records the settings
void Settings::saveSettings()
{
  // Local settings
  QSettings settings;
  if(ui->name_edit->text() != "")
    settings.setValue("local/Name",         ui->name_edit->text());
  if(ui->username_edit->text() != "")
    settings.setValue("local/Username",     ui->username_edit->text());

  // Video settings
  settings.setValue("video/Preset",       ui->preset->currentText());

  if(ui->scaled_height->text() != "")
    settings.setValue("video/ScaledHeight", ui->scaled_height->text());
  if(ui->scaled_width->text() != "")
    settings.setValue("video/ScaledWidth",  ui->scaled_width->text());
  if(ui->threads->text() != "")
    settings.setValue("video/Threads",      ui->threads->text());

  settings.setValue("video/QP",             QString::number(ui->qp->value()));

  if(ui->wpp->isChecked())
    settings.setValue("video/WPP",          "1");
  else
    settings.setValue("video/WPP",          "0");

  if(ui->vps->text() != "")
    settings.setValue("video/VPS",          ui->vps->text());
  if(ui->intra->text() != "")
    settings.setValue("video/Intra",        ui->intra->text());

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
    ui->name_edit->setText      (settings.value("local/Name").toString());
    ui->username_edit->setText  (settings.value("local/Username").toString());

    int index = ui->preset->findText(settings.value("video/Preset").toString());
    if(index != -1)
      ui->preset->setCurrentIndex(index);
    ui->scaled_height->setText  (settings.value("video/ScaledHeight").toString());
    ui->scaled_width->setText   (settings.value("video/ScaledWidth").toString());
    ui->threads->setText        (settings.value("video/Threads").toString());
    ui->qp->setValue            (settings.value("video/QP").toInt());

    if(settings.value("video/WPP").toString() == "1")
      ui->wpp->setChecked(true);
    else if(settings.value("video/WPP").toString() == "0")
      ui->wpp->setChecked(false);
    ui->vps->setText            (settings.value("video/VPS").toString());
    ui->intra->setText          (settings.value("video/Intra").toString());
  }
  else
  {
    // Some settings have not been initialized (due to new settings). Use defaults in GUI
    qDebug() << "Resettings settings to defaults";
    saveSettings();
  }
}
