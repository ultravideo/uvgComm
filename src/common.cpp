
#include "common.h"

// Didn't find sleep in QCore
#ifdef Q_OS_WIN
#include <winsock2.h> // for windows.h
#include <windows.h> // for Sleep
#endif

#include <QDebug>

// TODO move this to a different file from common.h
void qSleep(int ms)
{

#ifdef Q_OS_WIN
    Sleep(uint(ms));
#else
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000 * 1000 };
    nanosleep(&ts, nullptr);
#endif
}


//TODO use cryptographically secure callID generation to avoid collisions.
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


void printDebugObject(DebugType type, QObject* object,
                      QString context, QString description,
                      QStringList valueNames, QStringList values)
{
  printDebug(type, object->metaObject()->className(), context, description, valueNames, values);
}

void printDebug(DebugType type, QString className, QString context, QString description, QStringList valueNames, QStringList values)
{

  QString valueString = "";
  if (valueNames.size() == values.size())
  {
    for (int i = 0; i < valueNames.size(); ++i)
    {
      valueString.append(valueNames.at(i));
      valueString.append(": ");
      valueString.append(values.at(i));
      if (i != valueNames.size() - 1)
      {
        valueString.append("\r\n");
      }
    }
  }
  else {
    qDebug() << "Value printing failed";
  }

  // This could be reduced, but it might change so not worth probably at the moment.
  // Choose which text to print based on type.
  switch (type) {
  case DEBUG_NORMAL:
  {
    qDebug().nospace().noquote() << context << ", " << className << ": "
                                 << description << " " << valueString;
    break;
  }
  case DEBUG_ERROR:
  {
    qCritical().nospace().noquote() << "ERROR: --------------------------------------------";
    qCritical().nospace().noquote() << context << ", " << className << ": "
                                 << description << " " << valueString;
    qCritical().nospace().noquote() << "-------------------------------------------- ERROR";
    break;
  }
  case DEBUG_WARNING:
  {
    qWarning().nospace().noquote() << "WARNING: --------------------------------------------";
    qWarning().nospace().noquote() << context << ", " << className << ": "
                                 << description << " " << valueString;
    qWarning().nospace().noquote() << "-------------------------------------------- WARNING";
    break;
  }
  case DEBUG_PEER_ERROR:
  {
    qWarning().nospace().noquote() << "PEER ERROR: --------------------------------------------";
    qWarning().nospace().noquote() << context << ", " << className << ": "
                                 << description << " " << valueString;
    qWarning().nospace().noquote() << "-------------------------------------------- PEER ERROR";
    break;
  }

  }
}
