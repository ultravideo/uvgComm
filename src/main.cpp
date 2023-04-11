#include "kvazzupcontroller.h"

#include "settingskeys.h"
#include "logger.h"
#include "version.h"

#include <QApplication>
#include <QSettings>
#include <QFontDatabase>
#include <QDir>
#include <QMessageBox>
#include <QSystemTrayIcon>

int main(int argc, char *argv[])
{
  QApplication a(argc, argv);

#ifndef QT_NO_SYSTEMTRAYICON

  if (!QSystemTrayIcon::isSystemTrayAvailable()) {
      QMessageBox::critical(nullptr, QObject::tr("Systray"),
                            QObject::tr("couldn't detect any system tray "
                                        "on this system."));
      return 1;
  }
#endif

  a.setApplicationName("Kvazzup");
  //a.setQuitOnLastWindowClosed(false);

  Logger::getLogger()->printDebug(DEBUG_NORMAL, "Main", "Starting Kvazzup",
                                  {"Version"}, {QString::fromStdString(get_version())});

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
  QCoreApplication::setApplicationName("Kvazzup");

  QSettings::setPath(settingsFileFormat, QSettings::SystemScope, ".");

  QFile File(":/stylesheet.qss");
  File.open(QFile::ReadOnly);
  QString StyleSheet = QLatin1String(File.readAll());
  a.setStyleSheet(StyleSheet);

  KvazzupController controller;
  controller.init();

  return a.exec(); // starts main thread
}
