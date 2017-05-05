#include "audiocapturefilter.h"

#include <QDebug>
#include <QTime>


#include "statisticsinterface.h"

const int AUDIO_BUFFER_SIZE = 65536;

AudioCaptureFilter::AudioCaptureFilter(QString id, StatisticsInterface *stats) :
  Filter(id, "Audio_Capture", stats, false, true),
  deviceInfo_(QAudioDeviceInfo::defaultInputDevice()),
  device_(NULL),
  audioInput_(NULL),
  input_(NULL),
  buffer_(AUDIO_BUFFER_SIZE, 0)
{}

AudioCaptureFilter::~AudioCaptureFilter(){}

void AudioCaptureFilter::initializeAudio(QAudioFormat format)
{
  qDebug() << "Initializing audio capture filter";

  QAudioDeviceInfo info(deviceInfo_);
  if (!info.isFormatSupported(format)) {
    qWarning() << "Default format not supported - trying to use nearest";
    format_ = info.nearestFormat(format);
  }
  else
  {
    format_ = format;
  }

  stats_->audioInfo(format_.sampleRate(), format_.channelCount());

  if (device_)
    delete device_;
  device_  = new AudioCaptureDevice(format_, this);

  createAudioInput();
  qDebug() << "Audio initializing completed";
}

void AudioCaptureFilter::createAudioInput()
{
  qDebug() << "Creating audio input";
  audioInput_ = new QAudioInput(deviceInfo_, format_, this);

  device_->start();

  input_ = audioInput_->start();
  connect(input_, SIGNAL(readyRead()), SLOT(readMore()));

}

void AudioCaptureFilter::readMore()
{
  if (!audioInput_)
  {
    qWarning() << "No audio input in readMore";
    return;
  }
  qint64 len = audioInput_->bytesReady();
  if (len > AUDIO_BUFFER_SIZE)
  {
    len = AUDIO_BUFFER_SIZE;
  }
  qint64 l = input_->read(buffer_.data(), len);

  if (l > 0)
  {
    device_->write(buffer_.constData(), l);

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
  qDebug() << "Resuming audio input.";

  if (audioInput_->state() == QAudio::SuspendedState
      || audioInput_->state() == QAudio::StoppedState)
  {
    audioInput_->resume();
  }
}

void AudioCaptureFilter::stop()
{
  qDebug() << "Suspending audio input.";

  if (audioInput_->state() == QAudio::ActiveState)
  {
    audioInput_->suspend();
  }

  // just in case the filter part was running
  Filter::stop();

  qDebug() << "Audio input suspended.";
}

// changing of audio device mid stream.
void AudioCaptureFilter::deviceChanged(int index)
{
  qWarning() << "WARNING: audiocapturefilter device change not implemented fully:" << index;

  device_->stop();
  audioInput_->stop();
  audioInput_->disconnect(this);
  delete audioInput_;

  initializeAudio(format_);
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
