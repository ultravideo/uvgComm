#include "callwindow.h"

#include <QApplication>



int main(int argc, char *argv[])
{
  QApplication a(argc, argv);
  a.setApplicationName("HEVC Conferencing");

  // TODO load settings file

  QCoreApplication::setOrganizationName("Ultra Video Group");
  QCoreApplication::setOrganizationDomain("ultravideo.cs.tut.fi");
  QCoreApplication::setApplicationName("HEVC Conferencing");

  CallWindow call(NULL);

  call.show();
  call.startStream();

  return a.exec();
}
