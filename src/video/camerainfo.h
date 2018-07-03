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
  void getCapability(int deviceIndex, uint16_t format, uint16_t resolutionIndex,
                     QSize& resolution, double& framerate, QString &formatText);

  // used by dshow camera to determine the index of capability
  int formatToCapability(uint16_t device, uint16_t formatIndex, uint16_t resolutionIndex);

private:


};
