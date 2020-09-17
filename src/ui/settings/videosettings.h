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

  // initializes the custom view with values from settings.
  void init(int deviceID);

  void changedDevice(uint16_t deviceIndex);
  void resetSettings(int deviceID);

signals:

  void customSettingsChanged();
  void hidden();

public slots:

  // Slots related to parameter list
  void showParameterContextMenu(const QPoint& pos);
  void deleteListParameter();

  // button slots, called automatically by Qt
  void on_custom_ok_clicked();
  void on_custom_close_clicked();

  void on_add_parameter_clicked();

  void initializeResolutions(QString format);
  void initializeFramerates(QString resolution);

private:
  // QSettings -> GUI
  void restoreCustomSettings();

  void restoreComboBoxes();
  void restoreFormat();
  void restoreResolution();
  void restoreFramerate();

  void restoreComboBoxValue(QString key, QComboBox* box,
                            QString defaultValue);

  // GUI -> QSettings
  void saveCustomSettings();

  void saveCameraCapabilities(int deviceIndex);

  // initializes the UI with correct formats and resolutions
  void initializeFormat();

  bool checkVideoSettings();
  bool checkAudioSettings();


  int currentDevice_;

  Ui::VideoSettings *customUI_;

  std::shared_ptr<CameraInfo> cam_;

  QSettings settings_;
};
