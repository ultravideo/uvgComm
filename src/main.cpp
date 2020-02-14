#include "kvazzupcontroller.h"

#include "common.h"

#include <QApplication>
#include <QSettings>
#include <QFontDatabase>
#include <QDir>

int main(int argc, char *argv[])
{
  QApplication a(argc, argv);

  a.setApplicationName("Kvazzup");

  printDebug(DEBUG_NORMAL, "Main", "Starting Kvazzup.",
    {"Location"}, {QDir::currentPath()});

  // TODO move to GUI
  int id = QFontDatabase::addApplicationFont(QDir::currentPath() + "/fonts/OpenSans-Regular.ttf");

  if(id != -1)
  {
    QString family = QFontDatabase::applicationFontFamilies(id).at(0);
    a.setFont(QFont(family));
  }
  else
  {
    printDebug(DEBUG_WARNING, "Main", "Could not find default font. Is the file missing?");
  }

  QCoreApplication::setOrganizationName("Ultra Video Group");
  QCoreApplication::setOrganizationDomain("ultravideo.cs.tut.fi");
  QCoreApplication::setApplicationName("Kvazzup");

  QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

  QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, ".");

  QFile File("stylesheet.qss");
  File.open(QFile::ReadOnly);
  QString StyleSheet = QLatin1String(File.readAll());
  a.setStyleSheet(StyleSheet);

  KvazzupController controller;
  controller.init();

  return a.exec(); // starts main thread
}
