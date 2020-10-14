#include "audiosettings.h"

#include "ui_audiosettings.h"

#include "microphoneinfo.h"
#include "settingshelper.h"

#include "common.h"


AudioSettings::AudioSettings(QWidget* parent,
                             std::shared_ptr<MicrophoneInfo> info):
  QDialog(parent),
  currentDevice_(0),
  audioSettingsUI_(new Ui::AudioSettings),
  mic_(info),
  settings_("kvazzup.ini", QSettings::IniFormat)
{
  audioSettingsUI_->setupUi(this);

  connect(audioSettingsUI_->bitrate_slider, &QSlider::valueChanged,
          this, &AudioSettings::updateBitrate);

  connect(audioSettingsUI_->complexity_slider, &QSlider::valueChanged,
          this, &AudioSettings::updateComplexity);

  connect(audioSettingsUI_->bitrate_slider, &QSlider::valueChanged,
          audioSettingsUI_->audio_ok, &QPushButton::show);

  connect(audioSettingsUI_->complexity_slider, &QSlider::valueChanged,
          audioSettingsUI_->audio_ok, &QPushButton::show);

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
  //initializeChannelList();

  audioSettingsUI_->audio_ok->hide();

  if (checkSettings())
  {
    //restoreComboBoxValue("audio/channels", audioSettingsUI_->channel_combo, QString::number(1), settings_);

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

  audioSettingsUI_->audio_ok->hide();

  //saveTextValue("audio/channels",
  //              audioSettingsUI_->channel_combo->currentText(), settings_);

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

  if(// !settings_.contains("audio/channels") ||
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


void AudioSettings::initializeChannelList()
{
  //audioSettingsUI_->channel_combo->clear();
  QList<int> channels = mic_->getChannels(currentDevice_);

  for (int channel : channels)
  {
    //audioSettingsUI_->channel_combo->addItem(QString::number(channel));
  }
}
