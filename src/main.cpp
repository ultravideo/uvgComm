#include "callwindow.h"

#include <QApplication>
#include <QFontDatabase>
#include <QDir>

int main(int argc, char *argv[])
{
  QApplication a(argc, argv);

  a.setApplicationName("Kvazzup");

  // TODO load settings file

  qDebug() << "Starting Kvazzup in" << QDir::currentPath();

  int id = QFontDatabase::addApplicationFont(QDir::currentPath() + "/fonts/OpenSans-Regular.ttf");

  if(id != -1)
  {
    QString family = QFontDatabase::applicationFontFamilies(id).at(0);
    a.setFont(QFont(family));
  }
  else
  {
    qWarning() << "Could not find default font. Is the file missing?";
  }

  QCoreApplication::setOrganizationName("Ultra Video Group");
  QCoreApplication::setOrganizationDomain("ultravideo.cs.tut.fi");
  QCoreApplication::setApplicationName("Kvazzup");

  CallWindow call(NULL);

  call.show();
  call.startStream();

  return a.exec();
}
