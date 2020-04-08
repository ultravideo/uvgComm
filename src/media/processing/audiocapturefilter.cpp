#include "audiocapturefilter.h"

#include "statisticsinterface.h"

#include "common.h"

#include <QAudioInput>
#include <QTime>
#include <QSettings>
#include <QRegularExpression>


const int AUDIO_BUFFER_SIZE = 65536;

AudioCaptureFilter::AudioCaptureFilter(QString id, QAudioFormat format, StatisticsInterface *stats) :
  Filter(id, "Audio_Capture", stats, NONE, RAWAUDIO),
  deviceInfo_(),
  format_(format),
  audioInput_(nullptr),
  input_(nullptr),
  buffer_(AUDIO_BUFFER_SIZE, 0),
  wantedState_(QAudio::StoppedState)
{}


AudioCaptureFilter::~AudioCaptureFilter(){}


bool AudioCaptureFilter::init()
{
  printNormal(this, "Initializing audio capture filter.");

  QList<QAudioDeviceInfo> microphones = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);

  if (!microphones.empty())
  {
    QSettings settings("kvazzup.ini", QSettings::IniFormat);
    QString deviceName = settings.value("audio/Device").toString();
    int deviceID = settings.value("audio/DeviceID").toInt();

    if (deviceID < microphones.size())
    {
      QString parsedName = microphones[deviceID].deviceName();
      // take only the device name from: "Microphone (device name)"
      QRegularExpression re_mic (".*\\((.+)\\).*");
      QRegularExpressionMatch mic_match = re_mic.match(microphones[deviceID].deviceName());

      if (mic_match.hasMatch() && mic_match.lastCapturedIndex() == 1)
      {
        // parsed extra text succesfully
        parsedName = mic_match.captured(1);
      }

      // if the device has changed between recording the settings and now.
      if (parsedName != deviceName)
      {
        // search for device with same name
        for(int i = 0; i < microphones.size(); ++i)
        {
          if(parsedName == deviceName)
          {
            deviceID = i;
            printDebug(DEBUG_NORMAL, this, "Found Mic.", {"Name", "ID"},
                        {microphones.at(i).deviceName(), QString::number(deviceID)});
            break;
          }
        }
        // previous camera could not be found, use first.
        printWarning(this, "Did not find selected microphone. Using first.",
                    {"Device name"}, {deviceName});
        deviceID = 0;
      }
    }
    deviceInfo_ = microphones.at(deviceID);
  }
  else
  {
    printWarning(this, "No available microphones found. Trying default.");
    deviceInfo_ = QAudioDeviceInfo::defaultInputDevice();
  }

  QAudioDeviceInfo info(deviceInfo_);

  printNormal(this, "A microphone chosen.", {"Device name"}, {info.deviceName()});

  if (!info.isFormatSupported(format_)) {
    printWarning(this, "Default audio format not supported - trying to use nearest");
    format_ = info.nearestFormat(format_);
  }

  if(format_.sampleRate() != -1)
    getStats()->audioInfo(format_.sampleRate(), format_.channelCount());
  else
    getStats()->audioInfo(0, 0);

  createAudioInput();
  printNormal(this, "Audio initializing completed.");
  return true;
}


void AudioCaptureFilter::createAudioInput()
{

  audioInput_ = new QAudioInput(deviceInfo_, format_, this);
  if (audioInput_)
  {
    input_ = audioInput_->start();
    if (input_)
    {
      connect(input_, SIGNAL(readyRead()), SLOT(readMore()));
    }
  }
  wantedState_ = QAudio::ActiveState;

  connect(audioInput_, &QAudioInput::stateChanged,
          this,        &AudioCaptureFilter::stateChanged);

  printNormal(this, "Creating audio input.", {"Notify interval (ms)"},
              {QString::number(audioInput_->notifyInterval())});
}


void AudioCaptureFilter::readMore()
{
  if (!audioInput_ || !input_)
  {
    printProgramWarning(this,  "No audio input in readMore");
    return;
  }
  qint64 len = audioInput_->bytesReady();
  if (len > AUDIO_BUFFER_SIZE)
  {
    printWarning(this, "More data available than our buffer size!", {"Data available"},
                {QString::number(len) + "/" + QString::number(AUDIO_BUFFER_SIZE)});
    len = AUDIO_BUFFER_SIZE;
  }
  qint64 readData = input_->read(buffer_.data(), len);

  if (readData > 0)
  {
    Data* newSample = new Data;

    // create audio data packet to be sent to filter graph
    newSample->presentationTime.tv_sec = QDateTime::currentMSecsSinceEpoch()/1000;
    newSample->presentationTime.tv_usec = (QDateTime::currentMSecsSinceEpoch()%1000) * 1000;
    newSample->type = RAWAUDIO;
    newSample->data = std::unique_ptr<uint8_t[]>(new uint8_t[len]);

    memcpy(newSample->data.get(), buffer_.constData(), len);

    newSample->data_size = len;
    newSample->width = 0;
    newSample->height = 0;
    newSample->source = LOCAL;
    newSample->framerate = format_.sampleRate();

    std::unique_ptr<Data> u_newSample( newSample );
    sendOutput(std::move(u_newSample));

    //printNormal(this, "sent forward audio sample", {"Size"}, {QString::number(readData)});
  }
  else if (readData == 0)
  {
    printNormal(this, "No data given from microphone. Maybe the stream ended?");
  }
  else if (readData == -1)
  {
    printWarning(this, "Error reading data from mic IODevice!",
                  {"Amount"}, {QString::number(len)});
  }
}


void AudioCaptureFilter::start()
{
  printNormal(this, "Resuming audio input.");

  wantedState_ = QAudio::ActiveState;
  if (audioInput_ && (audioInput_->state() == QAudio::SuspendedState
      || audioInput_->state() == QAudio::StoppedState))
  {
    audioInput_->resume();
  }

}


void AudioCaptureFilter::stop()
{
  printNormal(this, "Suspending input.");

  wantedState_ = QAudio::SuspendedState;
  if (audioInput_ && audioInput_->state() == QAudio::ActiveState)
  {
    audioInput_->suspend();
  }

  // just in case the filter part was running
  Filter::stop();
  printNormal(this, "Input suspended.");
}


// changing of audio device mid stream.
void AudioCaptureFilter::updateSettings()
{
  printNormal(this, "Updating audio capture settings");

  if (audioInput_)
  {
    audioInput_->stop();
    audioInput_->disconnect(this);
    delete audioInput_;
    input_ = nullptr;
  }

  init();
}


void AudioCaptureFilter::volumeChanged(int value)
{
  if(audioInput_)
  {
    audioInput_->setVolume(qreal(value) / 100);
  }
}

void AudioCaptureFilter::stateChanged()
{
  printNormal(this, "AUdio Input State changed", {"States:"}, {
                "Current: " + QString::number(audioInput_->state()) + ", Wanted: " + QString::number(wantedState_)});

  if (audioInput_ && audioInput_->state() != wantedState_)
  {
    if (wantedState_ == QAudio::SuspendedState)
    {
      audioInput_->suspend();
    }
    else if (wantedState_ == QAudio::ActiveState)
    {
      if (audioInput_->state() == QAudio::StoppedState)
      {
        createAudioInput();
      }
      else if (audioInput_->state() == QAudio::SuspendedState)
      {
        audioInput_->resume();
      }
      else if (audioInput_->state() == QAudio::IdleState)
      {

      }
    }
  }
}


void AudioCaptureFilter::process()
{}
