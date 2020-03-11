#include "audiocapturefilter.h"

#include "statisticsinterface.h"

#include "common.h"

#include <QAudioInput>
#include <QDebug>
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
  buffer_(AUDIO_BUFFER_SIZE, 0)
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
            qDebug() << "Found mic with name:" << microphones.at(i).deviceName()
                     << "and id:" << i;
            deviceID = i;
            break;
          }
        }
        // previous camera could not be found, use first.
        qDebug() << "Did not find microphone name:" << deviceName << " Using first";
        deviceID = 0;
      }
    }
    deviceInfo_ = microphones.at(deviceID);
  }
  else
  {
    printWarning(this, "No available microphones found. Trying default");
    deviceInfo_ = QAudioDeviceInfo::defaultInputDevice();
  }

  QAudioDeviceInfo info(deviceInfo_);

  printDebug(DEBUG_NORMAL, this, "", {"Chosen Device"}, {info.deviceName()});

  if (!info.isFormatSupported(format_)) {
    printDebug(DEBUG_WARNING, this, "Default audio format not supported - trying to use nearest");
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
  qDebug() << "Iniating," << metaObject()->className() << ": Creating audio input";
  audioInput_ = new QAudioInput(deviceInfo_, format_, this);

  if (audioInput_)
    input_ = audioInput_->start();
  if (input_)
    connect(input_, SIGNAL(readyRead()), SLOT(readMore()));
}


void AudioCaptureFilter::readMore()
{
  if (!audioInput_)
  {
    printDebug(DEBUG_WARNING, this,  "No audio input in readMore");
    return;
  }
  qint64 len = audioInput_->bytesReady();
  if (len > AUDIO_BUFFER_SIZE)
  {
    len = AUDIO_BUFFER_SIZE;
  }
  quint64 l = 0;

  if (input_)
    l = input_->read(buffer_.data(), len);

  if (l > 0)
  {

    Data* newSample = new Data;

    // create audio data packet to be sent to filter graph
    timeval present_time;
    present_time.tv_sec = QDateTime::currentMSecsSinceEpoch()/1000;
    present_time.tv_usec = (QDateTime::currentMSecsSinceEpoch()%1000) * 1000;
    newSample->presentationTime = present_time;
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

    //qDebug() << "Audio capture: Generated sample with size:" << l;
  }
}


void AudioCaptureFilter::start()
{
  qDebug() << "Audio," << metaObject()->className() << ": Resuming audio input.";

  if (audioInput_ && (audioInput_->state() == QAudio::SuspendedState
      || audioInput_->state() == QAudio::StoppedState))
  {
    audioInput_->resume();
  }
}


void AudioCaptureFilter::stop()
{
  qDebug() << "Audio," << metaObject()->className() << ": Suspending input.";

  if (audioInput_ && audioInput_->state() == QAudio::ActiveState)
  {
    audioInput_->suspend();
  }

  // just in case the filter part was running
  Filter::stop();

  qDebug() << "Audio," << metaObject()->className() << ": input suspended.";
}


// changing of audio device mid stream.
void AudioCaptureFilter::updateSettings()
{
  printNormal(this, "Updating audio settings");

  if (audioInput_)
  {
    audioInput_->stop();
    audioInput_->disconnect(this);
   delete audioInput_;
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


void AudioCaptureFilter::process()
{}
