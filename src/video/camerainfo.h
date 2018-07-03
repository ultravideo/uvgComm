#pragma once

#include <QStringList>
#include <QSize>


// A helper class for settings. Meant to be an interface which later to
// replace with QCamera instead of ddshow.

class CameraInfo
{
public:
  CameraInfo();

  QStringList getVideoDevices();
  void getVideoCapabilities(int deviceID, QStringList& formats, QList<QStringList>& resolutions);
  void getCapability(int deviceIndex, int capabilityIndex,
                     QSize& resolution, double& framerate, QString &format);

};
