
#include "common.h"
#include "settingskeys.h"

#include "logger.h"

// Didn't find sleep in QCore
#ifdef Q_OS_WIN
#include <winsock2.h> // for windows.h
#include <windows.h> // for Sleep
#endif

#include <QSettings>
#include <QMutex>


const QString alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                         "abcdefghijklmnopqrstuvwxyz"
                         "0123456789";


QString generateRandomString(uint32_t length)
{
  // TODO make this cryptographically secure to avoid collisions
  QString string;
  for( unsigned int i = 0; i < length; ++i )
  {
    string.append(alphabet.at(qrand()%alphabet.size()));
  }
  return string;
}


bool settingEnabled(QString key)
{
  return settingValue(key) == 1;
}


int settingValue(QString key)
{
  QSettings settings(settingsFile, settingsFileFormat);

  if (!settings.value(key).isValid())
  {
    Logger::getLogger()->printDebug(DEBUG_WARNING, "Common", "Found faulty setting", 
                                    {"Key"}, {key});
    return 0;
  }

  return settings.value(key).toInt();
}


QString settingString(QString key)
{
  QSettings settings(settingsFile, settingsFileFormat);

  if (!settings.value(key).isValid())
  {
    Logger::getLogger()->printDebug(DEBUG_WARNING, "Common", "Found faulty setting", 
                                   {"Key"}, {key});
    return "";
  }

  return settings.value(key).toString();
}


QString getLocalUsername()
{
  QSettings settings(settingsFile, settingsFileFormat);

  return !settings.value(SettingsKey::localUsername).isNull()
      ? settings.value(SettingsKey::localUsername).toString() : "anonymous";
}


