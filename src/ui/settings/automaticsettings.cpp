#include "automaticsettings.h"
#include "ui_automaticsettings.h"

#include "roiarea.h"

#include "settingskeys.h"

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
  roi_.show();
  settings_.setValue(SettingsKey::manualROIStatus,          "1");

  emit updateAutomaticSettings();
}

void AutomaticSettings::roiAreaClosed()
{
  settings_.setValue(SettingsKey::manualROIStatus,          "0");
  emit updateAutomaticSettings();
}


VideoWidget* AutomaticSettings::getRoiSelfView()
{
  return roi_.getSelfVideoWidget();
}
