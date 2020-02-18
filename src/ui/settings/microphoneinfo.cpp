#include "microphoneinfo.h"

#include <QAudioDeviceInfo>
#include <QRegularExpression>

#include <common.h>


MicrophoneInfo::MicrophoneInfo()
{}


QStringList MicrophoneInfo::getDeviceList()
{
  QList<QAudioDeviceInfo> microphones
      = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);

  QStringList list;

  printDebug(DEBUG_NORMAL, "Microphone Info", "Get microhone list",
            {"Microphones"}, {QString::number(microphones.size())});

  for (int i = 0; i < microphones.size(); ++i)
  {
    // take only the device name from: "Microphone (device name)"
    QRegularExpression re_mic (".*\\((.+)\\).*");
    QRegularExpressionMatch mic_match = re_mic.match(microphones.at(i).deviceName());


    if (mic_match.hasMatch() && mic_match.lastCapturedIndex() == 1)
    {
      // parsed extra text succesfully
      list.push_back(mic_match.captured(1));
    }
    else
    {
      // did not find extra text. Using everything
      list.push_back(microphones.at(i).deviceName());
    }
  }

  return list;
}
