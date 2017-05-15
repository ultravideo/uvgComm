#include "callwindow.h"

#include <QApplication>



int main(int argc, char *argv[])
{
  QApplication a(argc, argv);
  a.setApplicationName("HEVC Conferencing");

  // TODO load settings file

  QString name = "";
  CallWindow call(NULL, 640, 480, name);

  call.show();
  call.startStream();

  return a.exec();
}
