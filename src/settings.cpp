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
  settings.setValue("local/Name", ui->name_edit->toPlainText());
  settings.setValue("local/Username", ui->username_edit->toPlainText());

  settings.setValue("video/ScaledHeight", ui->scaled_height->toPlainText());
  settings.setValue("video/ScaledWidth", ui->scaled_width->toPlainText());
  //settings.sync(); // TODO is this needed?
}

// restores temporarily recorded settings
void Settings::restoreSettings()
{
  QSettings settings;
  if(settings.contains("local/Name"))
  {
    ui->name_edit->setText(settings.value("local/Name").toString());
    ui->username_edit->setText(settings.value("local/Username").toString());

    ui->scaled_height->setText(settings.value("video/ScaledHeight").toString());
    ui->scaled_width->setText(settings.value("video/ScaledWidth").toString());
  }
  else
  {
    // No settings have bee initialized
    saveSettings();
  }
}
