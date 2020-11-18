#pragma once

#include <QDialog>
#include <QSettings>

#include <memory>

namespace Ui {
class AudioSettings;
}

class MicrophoneInfo;
class QSlider;

class AudioSettings : public QDialog
{
  Q_OBJECT
public:
  AudioSettings(QWidget* parent, std::shared_ptr<MicrophoneInfo> info);

  virtual void show();

  // initializes the custom view with values from settings.
  void init(int deviceID);

  void changedDevice(uint16_t deviceIndex);
  void resetSettings(int deviceID);

signals:

  void settingsChanged();
  void hidden();

public slots:

  // button slots, called automatically by Qt
  void on_audio_ok_clicked();
  void on_audio_close_clicked();

  void updateBitrate(int value);
  void updateComplexity(int value);
  void showOkButton(QString text);

private:
  // QSettings -> GUI
  void restoreSettings();

  // GUI -> QSettings
  void saveSettings();

  bool checkSettings();

  void initializeChannelList();

  int currentDevice_;

  Ui::AudioSettings *audioSettingsUI_;

  std::shared_ptr<MicrophoneInfo> mic_;

  QSettings settings_;

  QList<std::pair<QString, QSlider*>> sliders_;
};
