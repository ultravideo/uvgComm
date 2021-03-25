#include "audiosettings.h"

#include "ui_audiosettings.h"

#include "microphoneinfo.h"
#include "settingshelper.h"

#include "settingskeys.h"

#include "common.h"


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
  else
  {
    resetSettings(currentDevice_);
  }
}


void AudioSettings::saveSettings()
{
  printNormal(this, "Saving audio Settings");

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

bool AudioSettings::checkSettings()
{
  bool everythingOK = checkMissingValues(settings_);

  for (auto& slider : sliders_)
  {
    if (!settings_.contains(slider.first))
    {
      printError(this, "Missing a slider settings value.");
      everythingOK = false;
    }
  }

  for (auto& box : boxes_)
  {
    if (!settings_.contains(box.first))
    {
      printError(this, "Missing a box settings value.");
      everythingOK = false;
    }
  }

  if(// !settings_.contains(SettingsKey::audioChannels) ||
     !settings_.contains(SettingsKey::audioSignalType))
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
  /*
  audioSettingsUI_->channel_combo->clear();
  QList<int> channels = mic_->getChannels(currentDevice_);


  for (int channel : channels)
  {
    audioSettingsUI_->channel_combo->addItem(QString::number(channel));
  }
  */
}
