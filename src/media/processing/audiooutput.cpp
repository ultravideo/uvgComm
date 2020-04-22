#include "audiooutput.h"

#include "filter.h"
#include "statisticsinterface.h"

#include "common.h"

#include <QDateTime>

AudioOutput::AudioOutput(StatisticsInterface *stats):
  QObject(),
  stats_(stats),
  device_(QAudioDeviceInfo::defaultOutputDevice()),
  audioOutput_(nullptr),
  output_(nullptr),
  format_()
{}


AudioOutput::~AudioOutput()
{}


void AudioOutput::initializeAudio(QAudioFormat format)
{
  QAudioDeviceInfo info(device_);
  if (!info.isFormatSupported(format)) {
    printDebug(DEBUG_WARNING, this, "Default format not supported - trying to use nearest.");
    format_ = info.nearestFormat(format);
  }
  else
  {
    format_ = format;
  }

  createAudioOutput();
}


void AudioOutput::createAudioOutput()
{
  if(audioOutput_)
  {
    delete audioOutput_;
  }
  audioOutput_ = new QAudioOutput(device_, format_, this);

  // pull mode
  output_ = audioOutput_->start();
}


void AudioOutput::deviceChanged(int index)
{
  Q_UNUSED(index);
  printUnimplemented(this, "Audio output device change not implemented fully.");

  audioOutput_->stop();
  audioOutput_->disconnect(this);
  //initializeAudio(format_);
}


void AudioOutput::volumeChanged(int value)
{
  if (audioOutput_)
    audioOutput_->setVolume(qreal(value/100.0f));
}


void AudioOutput::takeInput(std::unique_ptr<Data> input, uint32_t sessionID)
{
  if (audioOutput_ && audioOutput_->state() != QAudio::StoppedState)
  {
    int audioChunks = audioOutput_->bytesFree()/audioOutput_->periodSize();

    printDebug(DEBUG_NORMAL, this, "Output values", {"input size", "audioOutput_->periodSize()", "output chunks"}, {
                 QString::number(input->data_size),
                 QString::number(audioOutput_->periodSize()),
                 QString::number(audioChunks)});

    const char *pointer = (char *)input->data.get();

    int dataLeft = input->data_size;
    while (audioChunks && dataLeft >= audioOutput_->periodSize())
    {
      qint64 written = output_->write(pointer, audioOutput_->periodSize());

      if (written == -1)
      {
        printError(this, "Audio output error");
        break;
      }
      dataLeft -= written;
      pointer += written;

      --audioChunks;
    }

    // Add audio delay to statistics
    int64_t delay = QDateTime::currentMSecsSinceEpoch() -
        ((uint64_t)input->presentationTime.tv_sec * 1000 +
         (uint64_t)input->presentationTime.tv_usec/1000);

    stats_->receiveDelay(sessionID, "Audio", delay);

    if (dataLeft >= audioOutput_->periodSize())
    {
      printWarning(this, "Audio output data overflow. "
                         "Output device cannot play this much audio. Discarding audio");
    }
    else if (dataLeft > 0 && audioChunks > 0)
    {
      printError(this, "Audio framesize is wrong. Discarding some part of it."
                       "It is recommended to use the same frame size as output period. "
                       "This could be fixed by recording the leftovers also.");
    }
  }
}


void AudioOutput::start()
{
  if(audioOutput_->state() == QAudio::SuspendedState
     || audioOutput_->state() == QAudio::StoppedState)
  {
    audioOutput_->resume();
  }
}


void AudioOutput::stop()
{
  if(audioOutput_->state() == QAudio::ActiveState)
  {
    audioOutput_->suspend();
  }
}
