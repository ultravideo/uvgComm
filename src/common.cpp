
#include "common.h"
#include "settingskeys.h"

#include "initiation/negotiation/sdptypes.h"

#include "logger.h"

// Didn't find sleep in QCore
#ifdef Q_OS_WIN
#include <winsock2.h> // for windows.h
#include <windows.h> // for Sleep
#endif

#include <QSettings>
#include <QMutex>
#include <cstdlib>


const QString alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                         "abcdefghijklmnopqrstuvwxyz"
                         "0123456789";


QString generateRandomString(uint32_t length)
{
  // TODO make this cryptographically secure to avoid collisions
  QString string;
  for( unsigned int i = 0; i < length; ++i )
  {
    string.append(alphabet.at(rand()%alphabet.size()));
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


bool getSendAttribute(const MediaInfo &media, bool local)
{
  bool send = false;
  if (local)
  {
    send = (media.flagAttributes.empty()
                 || media.flagAttributes.at(0) == A_NO_ATTRIBUTE
                 || media.flagAttributes.at(0) == A_SENDRECV
                 || media.flagAttributes.at(0) == A_SENDONLY);
  }
  else
  {
    send = (media.flagAttributes.empty()
                 || media.flagAttributes.at(0) == A_NO_ATTRIBUTE
                 || media.flagAttributes.at(0) == A_SENDRECV
                 || media.flagAttributes.at(0) == A_RECVONLY);
  }

  return send;
}


bool getReceiveAttribute(const MediaInfo &media, bool local)
{
  bool receive = false;
  if (local)
  {
    receive = (media.flagAttributes.empty()
                 || media.flagAttributes.at(0) == A_NO_ATTRIBUTE
                 || media.flagAttributes.at(0) == A_SENDRECV
                 || media.flagAttributes.at(0) == A_RECVONLY);
  }
  else
  {
    receive = (media.flagAttributes.empty()
                 || media.flagAttributes.at(0) == A_NO_ATTRIBUTE
                 || media.flagAttributes.at(0) == A_SENDRECV
                 || media.flagAttributes.at(0) == A_SENDONLY);
  }

  return receive;
}

