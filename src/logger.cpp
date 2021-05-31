#include "logger.h"


#include <QDebug>

const int BEGIN_LENGTH = 40;

std::shared_ptr<Logger> Logger::instance_ = nullptr;


Logger::Logger()
{}

std::shared_ptr<Logger> Logger::getLogger()
{
  if (Logger::instance_ == nullptr)
  {
    Logger::instance_ = std::shared_ptr<Logger>(new Logger());
  }

  return Logger::instance_;
}


void Logger::printDebug(DebugType type, const QObject *object, QString description,
                        QStringList valueNames, QStringList values)
{
  Logger::getLogger()->printDebug(type, object->metaObject()->className(),
             description, valueNames, values);
}


void Logger::printNormal(const QObject *object, QString description,
                         QString valueName, QString value)
{
  Logger::getLogger()->printDebug(DEBUG_NORMAL, object, description, {valueName}, {value});
}


void Logger::printImportant(const QObject* object, QString description,
                            QString valueName, QString value)
{
  Logger::getLogger()->printDebug(DEBUG_IMPORTANT, object, description, {valueName}, {value});
}


void Logger::printWarning(const QObject* object, QString description,
                          QString valueName, QString value)
{
  Logger::getLogger()->printDebug(DEBUG_WARNING, object, description, {valueName}, {value});
}


void Logger::printError(const QObject *object, QString description,
                        QString valueName, QString value)
{
  Logger::getLogger()->printDebug(DEBUG_ERROR, object, description, {valueName}, {value});
}


void Logger::printProgramError(const QObject *object, QString description,
                               QString valueName, QString value)
{
  Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, object, description, {valueName}, {value});
}


void Logger::printProgramWarning(const QObject *object, QString description,
                                 QString valueName, QString value)
{
  Logger::getLogger()->printDebug(DEBUG_PROGRAM_WARNING, object, description, {valueName}, {value});
}


void Logger::printPeerError(const QObject *object, QString description,
                            QString valueName, QString value)
{
  Logger::getLogger()->printDebug(DEBUG_PEER_ERROR, object, description, {valueName}, {value});
}


void Logger::printUnimplemented(const QObject* object, QString whatIsNotImplemented)
{
  Logger::getLogger()->printDebug(DEBUG_PROGRAM_WARNING, object,
             QString("NOT IMPLEMENTED: ") + whatIsNotImplemented);
}




void Logger::printDebug(DebugType type, QString className, QString description,
                        QStringList valueNames, QStringList values)
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
        if (valueNames.at(i) != "" && values.at(i) != "")
        {
          if (valueNames.size() != 1)
          {
            valueString.append(QString(BEGIN_LENGTH, ' '));
            valueString.append("-- ");
          }
          valueString.append(valueNames.at(i));
          valueString.append(": ");
          valueString.append(values.at(i));

          if (valueNames.size() != 1)
          {
            valueString.append("\r\n");
          }
        }
      }
    }
    else if (valueNames.size() == 1 && valueNames.at(0) != "") // if we have one name, add it
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
    }
    else if (valueNames.size() != values.size())
    {
      printDebug(DEBUG_PROGRAM_WARNING, "Logger",
                 "Debug printing could not figure how to print error values.",
                 {"Names", "Values"}, {QString::number(valueNames.size(), values.size())});
    }
  }

  // TODO: Set a constant length for everything before description.

  QString beginString = className + ": ";

  QString black   = "\033[0m";
  QString yellow  = "\033[1;33m";
  QString red     = "\033[31m";
  QString blue    = "\033[34m";

  // This could be reduced, but it might change so not worth probably at the moment.
  // Choose which text to print based on type.
  switch (type) {
  case DEBUG_NORMAL:
  {
    printHelper(black, beginString, valueString, description, valueNames.size());
    break;
  }
  case DEBUG_IMPORTANT:
  {
    printHelper(blue, beginString, valueString, description, valueNames.size(), true);
    break;
  }
  case DEBUG_ERROR:
  {
    printHelper(red, beginString, valueString, "ERROR! " + description, valueNames.size());
    break;
  }
  case DEBUG_WARNING:
  {
    printHelper(yellow, beginString, valueString, "Warning! " + description, valueNames.size());
    break;
  }
  case DEBUG_PEER_ERROR:
  {
    printHelper(red, beginString, valueString, "PEER ERROR: " + description, valueNames.size());
    break;
  }
  case DEBUG_PROGRAM_ERROR:
  {
    printHelper(red, beginString, valueString, "BUG: " + description, valueNames.size());
    break;
  }
  case DEBUG_PROGRAM_WARNING:
  {
    printHelper(yellow, beginString, valueString, "Minor bug: " + description, valueNames.size());
    break;
  }
  }
}


bool Logger::checkError(QObject* object, bool check, DebugType type,
                        QString description, QStringList values)
{
  Q_ASSERT(check);

  if (!check)
  {
    QStringList names;
    for (int i = 0; i < values.size(); ++i)
    {
      names.push_back("Value " + QString::number(i+1));
    }

    printDebug(type, object, description, names, values);
  }

  return check;
}


void Logger::printHelper(QString color, QString beginString, QString valueString,
                         QString description, int valuenames, bool emphasize)
{
  // TODO: Center text in middle for emphisized prints.
  if (beginString.length() < BEGIN_LENGTH)
  {
    beginString = beginString.leftJustified(BEGIN_LENGTH, ' ');
  }

  printMutex_.lock();
  QDebug printing = qDebug().nospace().noquote();

  if (emphasize)
  {
    qDebug();
    printing << color << "=============================================================================";
  }

  printing << color << beginString << description;
  if (!valueString.isEmpty())
  {
    // print one value on same line
    if (valuenames == 1)
    {
      printing << " (" << valueString << ")";
    }
    else // pring each value on separate line
    {
      printing << "\r\n" << valueString;
    }
  }

  if (emphasize)
  {
    printing << color << "=============================================================================";
    qDebug();
  }

  QString blackColor = "\033[0m";
  printing << blackColor;
  printMutex_.unlock();
}
