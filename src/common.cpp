
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


const int BEGIN_LENGTH = 40;


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


const std::map<DebugContext, QString> contextToString = {{DC_NO_CONTEXT, ""},
                                         {DC_STARTUP, "Startup"},
                                         {DC_SHUTDOWN, "Shutdown"},
                                         {DC_SETTINGS, "Settings"},
                                         {DC_START_CALL, "Starting call"},
                                         {DC_END_CALL, "Ending a call"},
                                         {DC_RINGING, "Ringing"},
                                         {DC_ACCEPT, "Accepting"},
                                         {DC_NEGOTIATING, "Negotiating the call"},
                                         {DC_SIP_CONTENT, "SIP Content"},
                                         {DC_ADD_MEDIA, "Creating Media"},
                                         {DC_REMOVE_MEDIA, "Removing Media"},
                                         {DC_PROCESS_MEDIA, "Processing Media"},
                                         {DC_AUDIO, "Audio"},
                                         {DC_FULLSCREEN, "Fullscreen"},
                                         {DC_DRAWING, "Drawing"},
                                         {DC_TCP, "TCP connection"},
                                         {DC_SEND_SIP, "Sending SIP"},
                                         {DC_SEND_SIP_REQUEST, "Sending SIP Request"},
                                         {DC_SEND_SIP_RESPONSE, "Sending SIP Response"},
                                         {DC_RECEIVE_SIP, "Receiving SIP"},
                                         {DC_RECEIVE_SIP_REQUEST, "Receiving SIP Request"},
                                         {DC_RECEIVE_SIP_RESPONSE, "Receiving SIP Response"}};



void printDebug(DebugType type, QObject* object,
                DebugContext context, QString description,
                QStringList valueNames, QStringList values)
{
  printDebug(type, object->metaObject()->className(), context, description, valueNames, values);
}

void printDebug(DebugType type, QString className, DebugContext context,
                QString description, QStringList valueNames, QStringList values)
{
  QString valueString = "";

  // do we have values.
  if( values.size() != 0)
  {
    // Add "name: value" because equal number of both.
    if (valueNames.size() == values.size()) // equal number of names and values
    {
      for (int i = 0; i < valueNames.size(); ++i)
      {
        valueString.append("-- ");
        valueString.append(valueNames.at(i));
        valueString.append(": ");
        valueString.append(values.at(i));
        if (i != valueNames.size() - 1)
        {
          valueString.append("\r\n");
        }
      }
    }
    else if (valueNames.size() == 1) // if we have one name, add it
    {
      valueString.append(valueNames.at(0));
      valueString.append(": ");
    }

    // If we have one or zero names, just add all values, unless we have 1 of both
    // in which case they were added earlier.
    if (valueNames.empty() || (valueNames.size() == 1 && values.size() != 1))
    {
      for (int i = 0; i < values.size(); ++i)
      {
        valueString.append(values.at(i));
        if (i != values.size() - 1)
        {
          valueString.append(", ");
        }
      }
      valueString.append("\r\n");
    }
    else if (valueNames.size() != values.size())
    {
      qDebug() << "Debug printing could not figure how to print error values."
               << "Names:" << valueNames.size()
               << "values: " << values.size();
    }
  }
  QString contextString = "No context";

  if (contextToString.find(context) != contextToString.end())
  {
    contextString = contextToString.at(context);
  }

  // TODO: Set a constant length for everything before description.


  QString beginString = contextString + ", " + className + ": ";

  if (beginString.length() < BEGIN_LENGTH)
  {
    beginString = beginString.leftJustified(BEGIN_LENGTH, ' ');
  }

  // This could be reduced, but it might change so not worth probably at the moment.
  // Choose which text to print based on type.
  switch (type) {
  case DEBUG_NORMAL:
  {
    QDebug printing = qDebug().nospace().noquote();
    printing << beginString << description;
    if (!valueString.isEmpty())
    {
      printing << "\r\n" << valueString;
    }
    break;
  }
  case DEBUG_IMPORTANT:
  {
    // TODO: Center text in middle.

    qDebug() << "==============================================================";
    qDebug().nospace().noquote() << beginString << description;
    if (!valueString.isEmpty())
    {
      qDebug().nospace().noquote() << valueString;
    }
    qDebug() << "==============================================================";
    break;
  }
  case DEBUG_ERROR:
  {
    qCritical() << "ERROR: " << description << " " << valueString;
    break;
  }
  case DEBUG_WARNING:
  {
    qWarning() << "Warning: " << description << " " << valueString;
    break;
  }
  case DEBUG_PEER_ERROR:
  {
    qWarning().nospace().noquote() << "PEER ERROR: --------------------------------------------";
    qWarning().nospace().noquote() << beginString << description;
    if (!valueString.isEmpty())
    {
      qWarning().nospace().noquote() << valueString;
    }
    qWarning().nospace().noquote() << "-------------------------------------------- PEER ERROR";
    break;
  }
  case DEBUG_PROGRAM_ERROR:
  {
    qCritical().nospace().noquote() << "BUG DETECTED: --------------------------------------------";
    qCritical().nospace().noquote() << beginString << description;
    if (!valueString.isEmpty())
    {
      qCritical().nospace().noquote() << valueString;
    }
    qCritical().nospace().noquote() << "-------------------------------------------- BUG";
    break;
  }
  case DEBUG_PROGRAM_WARNING:
  {
    qWarning().nospace().noquote() << "MINOR BUG DETECTED: --------------------------------------------";
    qWarning().nospace().noquote() << beginString << description;
    if (!valueString.isEmpty())
    {
      qWarning().nospace().noquote() << valueString;
    }
    qWarning().nospace() << "\r\n" << "-------------------------------------------- MINOR BUG";
    break;
  }
  }
}
