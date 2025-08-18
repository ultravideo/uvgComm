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


ScriptMode commandLine(QApplication& app, QString& outScriptfile, QString outConfigfile = "")
{
  QCommandLineParser parser;
  parser.setApplicationDescription("Video conferencing app with optional scripting support");
  parser.addHelpOption();
  parser.addVersionOption();

  QCommandLineOption scriptOption1("script",
                                  "Run script for automated testing. Use 'stdin' to read from stdin.",
                                  "filename");
  parser.addOption(scriptOption1);

  QCommandLineOption scriptOption2("config",
                                  "Use the specified config file instead of default.",
                                  "filename");
  parser.addOption(scriptOption2);

  parser.process(app);

  ScriptMode scriptMode = ScriptMode::SCRIPT_NONE;

  if (parser.isSet(scriptOption1))
  {
    QString scriptPath = parser.value(scriptOption1);
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

  if (parser.isSet(scriptOption2))
  {
    QString configPath = parser.value(scriptOption2);
    outConfigfile = configPath;
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

  ScriptMode script = commandLine(a, scriptFile, configFile);

  QFile File(":/stylesheet.qss");
  File.open(QFile::ReadOnly);
  QString StyleSheet = QLatin1String(File.readAll());
  a.setStyleSheet(StyleSheet);
  a.setWindowIcon(QIcon(":/favicon.ico"));

  uvgCommController controller;

  controller.init(script == ScriptMode::SCRIPT_STDIN, scriptFile, configFile);

  return a.exec(); // starts main thread
}
