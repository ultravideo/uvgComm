#include "screeninfo.h"

#include <QScreen>
#include <QGuiApplication>

ScreenInfo::ScreenInfo()
{

}


QStringList ScreenInfo::getDeviceList()
{
  QStringList screensList;
  QList<QScreen*> screens = QGuiApplication::screens();

  for (auto& screen : screens)
  {
    QString text = screen->name();

    if (screen->model() != "")
    {
      text = screen->model() + "(" + screen->name() + ")";
    }

    screensList.append(text);
  }

  return screensList;
}
