#include "controller.h"

#include "settingskeys.h"
#include "logger.h"

#include <QApplication>
#include <QSettings>
#include <QFontDatabase>
#include <QDir>
#include <QMessageBox>
//#include <QSystemTrayIcon>
#include <QCommandLineParser>

enum class ScriptMode
{
  SCRIPT_NONE,
  SCRIPT_STDIN,
  SCRIPT_FILE
};


ScriptMode commandLine(QApplication& app, QString& filename)
{
  QCommandLineParser parser;
  parser.setApplicationDescription("Video conferencing app with optional scripting support");
  parser.addHelpOption();
  parser.addVersionOption();

  QCommandLineOption scriptOption("script",
                                  "Run script for automated testing. Use '-' to read from stdin.",
                                  "file");
  parser.addOption(scriptOption);
  parser.process(app);

  if (parser.isSet(scriptOption))
  {
    QString scriptPath = parser.value(scriptOption);
    if (scriptPath == "-" || scriptPath == "stdin")
    {
      // Read from stdin
      return ScriptMode::SCRIPT_STDIN;
    }
    else
    {
      filename = scriptPath;
      return ScriptMode::SCRIPT_FILE;
    }
  }

  return ScriptMode::SCRIPT_NONE;
}


int main(int argc, char *argv[])
{
  QApplication a(argc, argv);

/*
#ifndef QT_NO_SYSTEMTRAYICON

  if (!QSystemTrayIcon::isSystemTrayAvailable()) {
      QMessageBox::critical(nullptr, QObject::tr("Systray"),
                            QObject::tr("couldn't detect any system tray "
                                        "on this system."));
      return 1;
  }
#endif
*/

  a.setApplicationName("uvgComm");
  //a.setQuitOnLastWindowClosed(false);

  //Logger::getLogger()->printDebug(DEBUG_NORMAL, "Main", "Starting uvgComm",
  //                                {"Version"}, {QString::fromStdString(get_version())});

  int id = QFontDatabase::addApplicationFont(":/fonts/OpenSans-Regular.ttf");

  if(id != -1)
  {
    QString family = QFontDatabase::applicationFontFamilies(id).at(0);
    a.setFont(QFont(family));
  }
  else
  {
    Logger::getLogger()->printDebug(DEBUG_WARNING, "Main", 
                                    "Could not find default font. Is the file missing?");
  }

  QCoreApplication::setOrganizationName("Ultra Video Group");
  QCoreApplication::setOrganizationDomain("ultravideo.fi");
  QCoreApplication::setApplicationName("uvgComm");

  QSettings::setPath(settingsFileFormat, QSettings::SystemScope, ".");

  QString file = "";

  ScriptMode script = commandLine(a, file);

  QFile File(":/stylesheet.qss");
  File.open(QFile::ReadOnly);
  QString StyleSheet = QLatin1String(File.readAll());
  a.setStyleSheet(StyleSheet);
  a.setWindowIcon(QIcon(":/favicon.ico"));

  uvgCommController controller;

  switch (script)
  {
    case ScriptMode::SCRIPT_FILE:
    {
      controller.init(file);
      break;
    }
    case ScriptMode::SCRIPT_STDIN:
    {
      controller.initStdin();
      break;
    }
    case ScriptMode::SCRIPT_NONE:
    {
      controller.init();
      break;
    }
  }

  return a.exec(); // starts main thread
}
