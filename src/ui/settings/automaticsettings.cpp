#include "automaticsettings.h"
#include "ui_automaticsettings.h"

#include "roiarea.h"

#include "settingskeys.h"
#include "logger.h"

AutomaticSettings::AutomaticSettings(QWidget *parent) :
  QDialog(parent),
  ui(new Ui::AutomaticSettings),
  roi_(),
  settings_(settingsFile, settingsFileFormat)
{
  ui->setupUi(this);
  QObject::connect(ui->close_button, &QPushButton::clicked,
                   this,             &AutomaticSettings::finished);

  QObject::connect(ui->roi_button, &QPushButton::clicked,
                   this,           &AutomaticSettings::showROI);

  QObject::connect(&roi_, &RoiArea::closed,
                   this, &AutomaticSettings::roiAreaClosed);

  settings_.setValue(SettingsKey::manualROIStatus,          "0");
  roi_.hide();
}


AutomaticSettings::~AutomaticSettings()
{
  delete ui;
}


void AutomaticSettings::finished()
{
  roi_.hide();
  hide();
  emit hidden();
}


void AutomaticSettings::showROI()
{
  Logger::getLogger()->printNormal(this, "Manual ROI window opened. "
                                         "Enabling manual ROI");
  roi_.show();
  settings_.setValue(SettingsKey::manualROIStatus,          true);

  emit updateAutomaticSettings();

  // TODO: Once rate control has been moved to automatic side, disable it here
}

void AutomaticSettings::roiAreaClosed()
{
  Logger::getLogger()->printNormal(this, "Manual ROI window closed. "
                                         "Disabling manual ROI");
  settings_.setValue(SettingsKey::manualROIStatus,          false);
  emit updateAutomaticSettings();
}


VideoWidget* AutomaticSettings::getRoiSelfView()
{
  return roi_.getSelfVideoWidget();
}
