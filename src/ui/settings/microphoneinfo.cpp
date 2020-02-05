#include "microphoneinfo.h"

#include <QAudioDeviceInfo>

MicrophoneInfo::MicrophoneInfo()
{}


QStringList MicrophoneInfo::getDeviceList()
{
  QList<QAudioDeviceInfo> microphones
      = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);

  QStringList list;

  for (int i = 0; i < microphones.size(); ++i)
  {
    list.push_back(microphones.at(i).deviceName());
  }

  return list;
}
