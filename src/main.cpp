#include "callwindow.h"

#include <QApplication>



int main(int argc, char *argv[])
{
  QApplication a(argc, argv);
  a.setApplicationName("HEVC Conferencing");

  // TODO load settings file

  CallWindow call(NULL);

  call.show();
  call.startStream();

  return a.exec();
}
