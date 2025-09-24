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



ScriptMode commandLine(QApplication& app, QString& outScriptfile, QString& outConfigfile, QString& statsFolder)
{
  QCommandLineParser parser;
  parser.setApplicationDescription("Video conferencing app with optional scripting support");
  parser.addHelpOption();
  parser.addVersionOption();

  QCommandLineOption scriptOption("script",
                                  "Run script for automated testing. Use 'stdin' to read from stdin.",
                                  "filename");
  parser.addOption(scriptOption);

  QCommandLineOption configOption("config",
                                  "Use the specified config file instead of default.",
                                  "filename");
  parser.addOption(configOption);

  QCommandLineOption statsOption("stats",
                                  "Record statistics into a folder instead of the window.",
                                  "destination folder");
  parser.addOption(statsOption);

  parser.process(app);

  ScriptMode scriptMode = ScriptMode::SCRIPT_NONE;

  if (parser.isSet(scriptOption))
  {
    QString scriptPath = parser.value(scriptOption);
    if (scriptPath == "-" || scriptPath == "stdin")
    {
      // Read from stdin
      scriptMode = ScriptMode::SCRIPT_STDIN;
    }
    else
    {
      outScriptfile = scriptPath;
      scriptMode = ScriptMode::SCRIPT_FILE;
    }
  }

  if (parser.isSet(configOption))
  {
    outConfigfile = parser.value(configOption);
  }

  if (parser.isSet(statsOption))
  {
    statsFolder = parser.value(statsOption);
  }

  return scriptMode;
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

  QString scriptFile = "";
  QString configFile = "";
  QString statsFolder = "";

  ScriptMode script = commandLine(a, scriptFile, configFile, statsFolder);

  QFile File(":/stylesheet.qss");
  File.open(QFile::ReadOnly);
  QString StyleSheet = QLatin1String(File.readAll());
  a.setStyleSheet(StyleSheet);
  a.setWindowIcon(QIcon(":/favicon.ico"));

  uvgCommController controller;

  controller.init(script == ScriptMode::SCRIPT_STDIN, scriptFile, configFile, statsFolder);

  return a.exec(); // starts main thread
}
