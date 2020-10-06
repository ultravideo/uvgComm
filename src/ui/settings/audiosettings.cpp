#include "audiosettings.h"

#include "ui_audiosettings.h"

#include "settingshelper.h"

#include "common.h"


AudioSettings::AudioSettings(QWidget* parent):
  QDialog(parent),
  currentDevice_(0),
  audioSettingsUI_(new Ui::AudioSettings),
  settings_("kvazzup.ini", QSettings::IniFormat)
{
  audioSettingsUI_->setupUi(this);

  connect(audioSettingsUI_->bitrate_slider, &QSlider::valueChanged,
          this, &AudioSettings::updateBitrate);

  connect(audioSettingsUI_->complexity_slider, &QSlider::valueChanged,
          this, &AudioSettings::updateComplexity);
}


void AudioSettings::init(int deviceID)
{
  currentDevice_ = deviceID;

  restoreSettings();

  updateBitrate(audioSettingsUI_->bitrate_slider->value());
  updateComplexity(audioSettingsUI_->complexity_slider->value());
}


void AudioSettings::changedDevice(uint16_t deviceIndex)
{
  currentDevice_ = deviceIndex;

  // TODO: set maximum number of channels based on device
}


void AudioSettings::resetSettings(int deviceID)
{
  currentDevice_ = deviceID;
  saveSettings();
}


// button slots, called automatically by Qt
void AudioSettings::on_audio_ok_clicked()
{
  printNormal(this, "Saving Audio Settings");
  saveSettings();
  emit settingsChanged();
}


void AudioSettings::on_audio_close_clicked()
{
  restoreSettings();
  hide();
  emit hidden();
}


void AudioSettings::restoreSettings()
{
  if (checkSettings())
  {
    unsigned int channels = settings_.value("audio/channels").toUInt();
    audioSettingsUI_->channels_box->setValue(channels);

    unsigned int bitrate = settings_.value("audio/bitrate").toUInt();
    audioSettingsUI_->bitrate_slider->setValue(bitrate);

    unsigned int complexity = settings_.value("audio/complexity").toUInt();
    audioSettingsUI_->complexity_slider->setValue(complexity);

    QString type = settings_.value("audio/signalType").toString();
    audioSettingsUI_->signal_combo->setCurrentText(type);
  }
  else
  {
    resetSettings(currentDevice_);
  }
}


void AudioSettings::saveSettings()
{
  printNormal(this, "Saving audio Settings");

  saveTextValue("audio/channels",
                QString::number(audioSettingsUI_->channels_box->value()),
                settings_);

  saveTextValue("audio/bitrate",
                {QString::number(audioSettingsUI_->bitrate_slider->value())},
                settings_);

  saveTextValue("audio/complexity",
                QString::number(audioSettingsUI_->complexity_slider->value()),
                settings_);

  saveTextValue("audio/signalType",
                audioSettingsUI_->signal_combo->currentText(), settings_);
}

bool AudioSettings::checkSettings()
{
  bool everythingOK = checkMissingValues(settings_);

  if(!settings_.contains("audio/channels") ||
     !settings_.contains("audio/bitrate") ||
     !settings_.contains("audio/complexity") ||
     !settings_.contains("audio/signalType"))
  {
    printError(this, "Missing an audio settings value.");
    everythingOK = false;
  }

  return everythingOK;
}


void AudioSettings::updateBitrate(int value)
{
  if (value == 0)
  {
    audioSettingsUI_->bitrate_label->setText("Bitrate (disabled)");
  }
  else
  {
    value = roundToThousands(value);

    audioSettingsUI_->bitrate_label->setText("Bitrate (" + getBitrateString(value) + ")");
    audioSettingsUI_->bitrate_slider->setValue(value);
  }
}

void AudioSettings::updateComplexity(int value)
{
  audioSettingsUI_->complexity_label->setText(
        "Complexity (" + QString::number(value) + ")");
}
