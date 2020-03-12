#pragma once

#include <QStringList>

#include "deviceinfointerface.h"

class ScreenInfo : public DeviceInfoInterface
{
public:
  ScreenInfo();

  // returns a list of devices.
  virtual QStringList getDeviceList();


};
