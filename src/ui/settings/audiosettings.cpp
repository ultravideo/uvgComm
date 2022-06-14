#include "audiosettings.h"

#include "ui_audiosettings.h"

#include "microphoneinfo.h"
#include "settingshelper.h"

#include "settingskeys.h"

#include "common.h"
#include "global.h"
#include "logger.h"


const QStringList neededSettings = {SettingsKey::audioBitrate,
                                    SettingsKey::audioComplexity,
                                    SettingsKey::audioSignalType,
                                    SettingsKey::audioAEC,
                                    SettingsKey::audioDenoise,
                                    SettingsKey::audioDereverb,
                                    SettingsKey::audioAGC};


AudioSettings::AudioSettings(QWidget* parent,
                             std::shared_ptr<MicrophoneInfo> info):
  QDialog(parent),
  currentDevice_(0),
  audioSettingsUI_(new Ui::AudioSettings),
  mic_(info),
  settings_(settingsFile, settingsFileFormat)
{
  audioSettingsUI_->setupUi(this);

  sliders_.push_back({SettingsKey::audioBitrate, audioSettingsUI_->bitrate_slider});
  sliders_.push_back({SettingsKey::audioComplexity, audioSettingsUI_->complexity_slider});

  sliders_.push_back({SettingsKey::audioAECDelay, audioSettingsUI_->aec_playback_delay});
  sliders_.push_back({SettingsKey::audioAECFilterLength, audioSettingsUI_->aec_filter_length});

  boxes_.push_back({SettingsKey::audioAEC, audioSettingsUI_->aec_box});
  boxes_.push_back({SettingsKey::audioDenoise, audioSettingsUI_->denoise_box});
  boxes_.push_back({SettingsKey::audioDereverb, audioSettingsUI_->dereverberation_box});
  boxes_.push_back({SettingsKey::audioAGC, audioSettingsUI_->agc_box});

  for (auto& slider : sliders_)
  {
    connect(slider.second, &QSlider::valueChanged,
            audioSettingsUI_->audio_ok, &QPushButton::show);
  }

  for (auto& box : boxes_)
  {
    connect(box.second, &QCheckBox::stateChanged,
            audioSettingsUI_->audio_ok, &QPushButton::show);
  }

  connect(audioSettingsUI_->bitrate_slider, &QSlider::valueChanged,
          this, &AudioSettings::updateBitrate);

  connect(audioSettingsUI_->complexity_slider, &QSlider::valueChanged,
          this, &AudioSettings::updateComplexity);

  connect(audioSettingsUI_->aec_playback_delay, &QSlider::valueChanged,
          this, &AudioSettings::updateAECDelay);

  connect(audioSettingsUI_->aec_filter_length, &QSlider::valueChanged,
          this, &AudioSettings::updateAECFilterLength);


  connect(audioSettingsUI_->signal_combo, &QComboBox::currentTextChanged,
          this, &AudioSettings::showOkButton);
}


void AudioSettings::show()
{
  audioSettingsUI_->audio_ok->hide();
  QDialog::show();
}

void AudioSettings::showOkButton(QString text)
{
  Q_UNUSED(text);
  audioSettingsUI_->audio_ok->show();
}


void AudioSettings::closeEvent(QCloseEvent *event)
{
  on_audio_close_clicked();
  QDialog::closeEvent(event);
}


void AudioSettings::init(int deviceID)
{
  currentDevice_ = deviceID;

  restoreSettings();

  updateBitrate(audioSettingsUI_->bitrate_slider->value());
  updateComplexity(audioSettingsUI_->complexity_slider->value());

  updateAECDelay(audioSettingsUI_->aec_playback_delay->value());
  updateAECFilterLength(audioSettingsUI_->aec_filter_length->value());
}


void AudioSettings::changedDevice(uint16_t deviceIndex)
{
  currentDevice_ = deviceIndex;

  // TODO: set maximum number of channels based on device
}


// button slots, called automatically by Qt
void AudioSettings::on_audio_ok_clicked()
{
  Logger::getLogger()->printNormal(this, "Saving Audio Settings");
  saveSettings();
  emit updateAudioSettings();
}


void AudioSettings::on_audio_close_clicked()
{
  restoreSettings();
  hide();
  emit hidden();
}


void AudioSettings::restoreSettings()
{
  //initializeChannelList();

  audioSettingsUI_->audio_ok->hide();

  //restoreComboBoxValue("audio/channels",
  // audioSettingsUI_->channel_combo, QString::number(1), settings_);

  for (auto& slider : sliders_)
  {
    unsigned int bitrate = settings_.value(slider.first).toUInt();
    slider.second->setValue(bitrate);
  }

  QString type = settings_.value(SettingsKey::audioSignalType).toString();
  audioSettingsUI_->signal_combo->setCurrentText(type);

  for (auto& box : boxes_)
  {
    restoreCheckBox(box.first, box.second, settings_);
  }
}


void AudioSettings::saveSettings()
{
  Logger::getLogger()->printNormal(this, "Saving audio Settings");

  audioSettingsUI_->audio_ok->hide();

  //saveTextValue(SettingsKey::audioChannels,
  //              audioSettingsUI_->channel_combo->currentText(), settings_);

  for (auto& slider : sliders_)
  {
    saveTextValue(slider.first, {QString::number(slider.second->value())}, settings_);
  }

  for (auto& box : boxes_)
  {
    saveCheckBox(box.first, box.second, settings_);
  }

  saveTextValue(SettingsKey::audioSignalType,
                audioSettingsUI_->signal_combo->currentText(), settings_);
}


void AudioSettings::updateBitrate(int value)
{
  if (value == 0)
  {
    audioSettingsUI_->bitrate_label->setText("Bitrate (disabled)");
  }
  else
  {
    value = roundToNumber(value, 1000);

    audioSettingsUI_->bitrate_label->setText("Bitrate (" + getBitrateString(value) + ")");
    audioSettingsUI_->bitrate_slider->setValue(value);
  }
}


void AudioSettings::updateComplexity(int value)
{
  audioSettingsUI_->complexity_label->setText(
        "Complexity (" + QString::number(value) + ")");
}


void AudioSettings::updateAECDelay(int value)
{
  setTimeValue(value, audioSettingsUI_->aec_playback_label,
               audioSettingsUI_->aec_playback_delay);
}


void AudioSettings::updateAECFilterLength(int value)
{
  setTimeValue(value, audioSettingsUI_->aec_filter_label,
               audioSettingsUI_->aec_filter_length);
}


void AudioSettings::setTimeValue(int value, QLabel* label, QSlider* slider)
{
  int rounded = roundToNumber(value, 1000/AUDIO_FRAMES_PER_SECOND);
  label->setText(QString::number(rounded) + " ms");
  slider->setValue(rounded);
}


void AudioSettings::initializeChannelList()
{
  /*
  audioSettingsUI_->channel_combo->clear();
  QList<int> channels = mic_->getChannels(currentDevice_);


  for (int channel : channels)
  {
    audioSettingsUI_->channel_combo->addItem(QString::number(channel));
  }
  */
}
