#include "settings.h"
#include "ui_settings.h"

#include <QDebug>

Settings::Settings(QWidget *parent) :
  QDialog(parent),
  ui(new Ui::Settings)
{
  ui->setupUi(this);
}

Settings::~Settings()
{
  delete ui;
}

void Settings::show()
{
  qDebug() << "Saving settings to temp";
  saveSettings();
  QDialog::show();
}

void Settings::cancel()
{
  qDebug() << "Getting settings from temp";
  restoreSettings();
  hide();
}

void Settings::getSettings(std::map<QString, QString>& settings)
{
  settings["LocalName"] = ui->name_edit->toPlainText();
  settings["LocalUsername"] = ui->username_edit->toPlainText();
  settings["ScaledHeight"] = ui->scaled_height->toPlainText();
  settings["ScaledWidth"] = ui->scaled_width->toPlainText();
}

// temparily records the settings
void Settings::saveSettings()
{
  tempSettings_.clear();
  getSettings(tempSettings_);
}

// restores temporarily recorded settings
void Settings::restoreSettings()
{
  if(tempSettings_.find("Name") == tempSettings_.end())
  {
    qDebug() << "WARNING: Did not find recorded settings in restoreSettings!";
    return;
  }
  ui->name_edit->setText(tempSettings_["Name"]);
  ui->username_edit->setText(tempSettings_["username"]);
  ui->scaled_height->setText(tempSettings_["ScaledHeight"]);
  ui->scaled_width->setText(tempSettings_["ScaledWidth"]);
}

// saves UI settings to file
void Settings::settingsToFile(QString filename)
{}

// loads settings from a fileto UI
void Settings::loadSettingsFromFile(QString filename)
{}
