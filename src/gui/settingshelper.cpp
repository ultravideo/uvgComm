#include "settingshelper.h"

#include <QCheckBox>
#include <QDebug>


void saveCheckBox(const QString settingValue, QCheckBox* box, QSettings& settings)
{
  if(box->isChecked())
  {
    settings.setValue(settingValue,          "1");
  }
  else
  {
    settings.setValue(settingValue,          "0");
  }
}


void restoreCheckBox(const QString settingValue, QCheckBox* box, QSettings& settings)
{
  if(settings.value(settingValue).toString() == "1")
  {
    box->setChecked(true);
  }
  else if(settings.value(settingValue).toString() == "0")
  {
    box->setChecked(false);
  }
  else
  {
    qDebug() << "Settings, SettingsHelper: Corrupted value for checkbox in settings file for:"
             << settingValue << "!!!";
  }
}


void saveTextValue(const QString settingValue, const QString &text, QSettings& settings)
{
  if(text != "")
  {
    settings.setValue(settingValue,  text);
  }
}


bool checkMissingValues(QSettings& settings)
{
  QStringList list = settings.allKeys();

  bool foundEverything = true;
  for(auto key : list)
  {
    if(settings.value(key).isNull() || settings.value(key) == "")
    {
      qDebug() << "Settings, SettingsHelper: MISSING SETTING FOR:" << key;
      foundEverything = false;
    }
  }
  return foundEverything;
}
