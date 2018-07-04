#pragma once

#include <QStringList>
#include <QSize>

// A helper class for settings. Meant to be an interface which later to
// replace with QCamera instead of ddshow, if possible

class CameraInfo
{
public:
  CameraInfo();

  // returns a list of devices. with devideID as the place in list
  QStringList getVideoDevices();

  // get formats and resolutions for a particular device.
  void getVideoCapabilities(int deviceID, QStringList& formats, QList<QStringList>& resolutions);

  // get exact capabilities of a particular capability choice. Can also be parsed from resolutions
  void getCapability(int deviceIndex, uint16_t format, uint16_t resolutionIndex,
                     QSize& resolution, double& framerate, QString &formatText);

  // used by dshow camera to determine the index of capability
  int formatToCapability(uint16_t device, uint16_t formatIndex, uint16_t resolutionIndex);

private:


};
