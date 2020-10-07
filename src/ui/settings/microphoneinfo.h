#pragma once

#include <QStringList>

#include "deviceinfointerface.h"


class MicrophoneInfo : public DeviceInfoInterface
{
public:
  MicrophoneInfo();

  // returns a list of devices.
  virtual QStringList getDeviceList();

  QList<int> getChannels(int deviceID);
};
