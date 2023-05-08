#include "microphoneinfo.h"

#include <common.h>

#include <QAudioDevice>
#include <QRegularExpression>
#include <QMediaDevices>


MicrophoneInfo::MicrophoneInfo()
{}


QStringList MicrophoneInfo::getDeviceList()
{
  const QList<QAudioDevice> microphones = QMediaDevices::audioInputs();
  QStringList list;

  //Logger::getLogger()->printDebug(DEBUG_NORMAL, "Microphone Info", "Get microhone list",
  //                                {"Microphones"}, {QString::number(microphones.size())});

  for (int i = 0; i < microphones.size(); ++i)
  {
      // take only the device name from: "Microphone (device name)"
      QRegularExpression re_mic (".*\\((.+)\\).*");
      QRegularExpressionMatch mic_match = re_mic.match(microphones.at(i).description());

      if (mic_match.hasMatch() && mic_match.lastCapturedIndex() == 1)
      {
        // parsed extra text succesfully
        list.push_back(mic_match.captured(1));
      }
      else
      {
        // did not find extra text. Using everything
        list.push_back(microphones.at(i).description());
      }
  }

  // the texts in this list must match exactly what the audio input is searching for
  return list;
}


QList<int> MicrophoneInfo::getChannels(int deviceID)
{
  if (deviceID != -1)
  {
    const auto microphones = QMediaDevices::audioInputs();

    if (!microphones.empty() && deviceID < microphones.size())
    {
        return {microphones.at(deviceID).minimumChannelCount(),
                microphones.at(deviceID).maximumChannelCount()};
    }
  }

  return {};
}
