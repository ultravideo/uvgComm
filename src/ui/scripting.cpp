#include "scripting.h"

#include "common.h"
#include "logger.h"

#include "settingskeys.h"
#include "participantinterface.h"

#include <QSettings>

Scripting::Scripting()
{}


void Scripting::fileScripting(const QString& filename)
{
  Logger::getLogger()->printNormal(this, "Running script from file: " + filename);
  filename_ = filename;
}


void Scripting::stdinScripting()
{
  Logger::getLogger()->printNormal(this, "Running script from stdin");
}



void Scripting::run()
{
  if (!filename_.isEmpty())
  {
    QFile file(filename_);
    if (file.open(QFile::ReadOnly | QFile::Text))
    {
      QTextStream stream(&file);
      processStream(stream);  // Process the commands from the file
    }
  }
  else
  {
    QTextStream stream(stdin);
    processStream(stream);  // Process commands from stdin
  }
}


void Scripting::processStream(QTextStream& stream)
{
  while (!stream.atEnd())
  {
    QString line = stream.readLine().trimmed();
    if (line.isEmpty() || line.startsWith("#"))  // Skip empty lines and comments
      continue;

    executeCommand(line);
  }
}

void Scripting::executeCommand(const QString& command)
{
  Logger::getLogger()->printNormal(this, "Executing: " + command);
  QStringList parts = command.split(" ", Qt::SkipEmptyParts);
  if (parts.isEmpty())
  {
    return;
  }

  QString cmd = parts[0];

  parts.removeFirst();  // Remove the command from the list

  if (cmd == "call")
  {
    if (parts.size() < 2)
    {
      Logger::getLogger()->printWarning(this, "Invalid call command: " + command);
      return;
    }

    QString username = parts[0];
    QString address = parts[1];

    QMetaObject::invokeMethod(interface_, [=]() {
      interface_->startINVITETransaction(username, username, address);
    }, Qt::QueuedConnection);
  }
  else if (cmd == "wait")
  {
    if (parts.size() < 1)
    {
      Logger::getLogger()->printWarning(this, "Invalid wait command: " + command);
      return;
    }

    bool ok;
    int waitTime = parts[0].toInt(&ok);
    if (ok)
    {
      Logger::getLogger()->printNormal(this, "Waiting for " + QString::number(waitTime) + " seconds");
      QThread::msleep(waitTime * 1000); // Convert seconds to milliseconds
    }
    else
    {
      Logger::getLogger()->printWarning(this, "Invalid wait time: " + parts[0]);
    }
  }
  else if (cmd == "hangup" || cmd == "end" || cmd == "endcall")
  {
    emit endCall();
  }
  else if (cmd == "setting" || cmd == "settingValue")
  {
    if (parts.size() < 2)
    {
      Logger::getLogger()->printWarning(this, "Invalid setting command: " + command);
      return;
    }

    QSettings settings = QSettings(getSettingsFile(), settingsFileFormat);
    settings.setValue(parts[0], parts[1]);
  }
  else if (cmd == "setVideo")
  {
    emit updateVideoSetting();
  }
  else if (cmd == "setAudio")
  {
    emit updateAudioSetting();
  }
  else if (cmd == "setCall")
  {
    emit updateCallSetting();
  }
  else if (cmd == "quit" || cmd == "exit" || cmd == "close")
  {
    emit quitScript();
    quit();
  }
  else if (cmd == "comment" || cmd.startsWith("#"))
  {
    // Do nothing, it's a comment
  }
  else
  {
    Logger::getLogger()->printWarning(this, "Unknown command: " + cmd);
  }
}
