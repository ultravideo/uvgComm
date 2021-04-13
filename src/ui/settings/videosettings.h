#pragma once

#include <QDialog>
#include <QSettings>

#include <memory>

// The basic logic for this class goes as following:
// 1) initialize fills UI elements with information so they can be interacted with
// 2) restore reads the from QSettings and sets the GUI values
// 3) save records the GUI values to QSettings.
// 4) reset records the default GUI values to QSettings.


namespace Ui {
class VideoSettings;
}

class CameraInfo;
class QComboBox;

class VideoSettings  : public QDialog
{
  Q_OBJECT
public:
  VideoSettings(QWidget* parent, std::shared_ptr<CameraInfo> info);

  // initializes the view with values from settings.
  void init(int deviceID);

  void changedDevice(uint16_t deviceIndex);
  void resetSettings(int deviceID);

  // GUI -> QSettings

  void saveCameraCapabilities(int deviceIndex);

signals:

  void settingsChanged();
  void hidden();

public slots:

  // Slots related to parameter list
  void showParameterContextMenu(const QPoint& pos);
  void deleteListParameter();

  // button slots, called automatically by Qt
  void on_video_ok_clicked();
  void on_video_close_clicked();

  void on_add_parameter_clicked();

  void refreshResolutions(int index);
  void refreshFramerates(int index);

  void updateBitrate(int value);

  void updateSliceBoxStatus();
  void updateTilesStatus();
  void updateObaStatus(int index);

private:
  void initializeResolutions();
  void initializeFramerates();

  // QSettings -> GUI
  void restoreSettings();

  void restoreComboBoxes();
  void restoreFormat();
  void restoreResolution();
  void restoreFramerate();

  // GUI -> QSettings
  void saveSettings();

  // initializes the UI with correct formats and resolutions
  void initializeFormat();
  void initializeThreads();

  int currentDevice_;

  Ui::VideoSettings *videoSettingsUI_;

  std::shared_ptr<CameraInfo> cam_;

  QSettings settings_;
};
