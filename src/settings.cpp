#include "settings.h"
#include "ui_settings.h"

#include <QDebug>

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

  QSettings settings;
  if(ui->name_edit->toPlainText() != "")
    settings.setValue("local/Name",         ui->name_edit->toPlainText());
  if(ui->username_edit->toPlainText() != "")
    settings.setValue("local/Username",     ui->username_edit->toPlainText());

  if(ui->preset->toPlainText() != "")
    settings.setValue("video/Preset",       ui->preset->toPlainText());
  if(ui->scaled_height->toPlainText() == "ultrafast"
     || ui->scaled_height->toPlainText() == "superfast"
     || ui->scaled_height->toPlainText() == "veryfast"
     || ui->scaled_height->toPlainText() == "faster"
     || ui->scaled_height->toPlainText() == "fast"
     || ui->scaled_height->toPlainText() == "medium"
     || ui->scaled_height->toPlainText() == "slow"
     || ui->scaled_height->toPlainText() == "slower"
     || ui->scaled_height->toPlainText() == "veryslow"
     || ui->scaled_height->toPlainText() == "placebo")
    settings.setValue("video/ScaledHeight", ui->scaled_height->toPlainText());
  if(ui->scaled_width->toPlainText() != "")
    settings.setValue("video/ScaledWidth",  ui->scaled_width->toPlainText());
  if(ui->threads->toPlainText() != "")
    settings.setValue("video/Threads",      ui->threads->toPlainText());
  if(ui->qp->toPlainText() != ""
     && ui->qp->toPlainText().toInt() >= 0
     && ui->qp->toPlainText().toInt() <= 51)
    settings.setValue("video/QP",           ui->qp->toPlainText());
  if(ui->wpp->toPlainText() != "")
    settings.setValue("video/WPP",          ui->wpp->toPlainText());
  if(ui->vps->toPlainText() != "")
    settings.setValue("video/VPS",          ui->vps->toPlainText());
  if(ui->intra->toPlainText() != "")
    settings.setValue("video/Intra",        ui->intra->toPlainText());

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
    if(!missingSettings && settings.value(key).isNull())
      missingSettings = true;
  }

  //get values from QSettings
  if(!missingSettings)
  {
    qDebug() << "Restoring preivous settings";
    ui->name_edit->setText      (settings.value("local/Name").toString());
    ui->username_edit->setText  (settings.value("local/Username").toString());

    ui->preset->setText         (settings.value("video/Preset").toString());
    ui->scaled_height->setText  (settings.value("video/ScaledHeight").toString());
    ui->scaled_width->setText   (settings.value("video/ScaledWidth").toString());
    ui->threads->setText        (settings.value("video/Threads").toString());
    ui->qp->setText             (settings.value("video/QP").toString());
    ui->wpp->setText            (settings.value("video/WPP").toString());
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
