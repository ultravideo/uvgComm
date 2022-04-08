#include "automaticsettings.h"
#include "ui_automaticsettings.h"

#include "settingskeys.h"
#include "logger.h"

enum TabType {
  MAIN_TAB = 0, ROI_TAB = 1
};

AutomaticSettings::AutomaticSettings(QWidget *parent):
  QDialog(parent),
  ui_(new Ui::AutomaticSettings),
  settings_(settingsFile, settingsFileFormat)
{
  ui_->setupUi(this);
  QObject::connect(ui_->close_button, &QPushButton::clicked,
                   this,             &AutomaticSettings::finished);

  QObject::connect(ui_->reset_button, &QPushButton::clicked,
                   this,             &AutomaticSettings::reset);

  QObject::connect(ui_->tabs, &QTabWidget::currentChanged,
                   this,      &AutomaticSettings::tabChanged);

  settings_.setValue(SettingsKey::manualROIStatus,          "0");
  ui_->roi_surface->enableOverlay();
}


AutomaticSettings::~AutomaticSettings()
{
  delete ui_;
}


void AutomaticSettings::show()
{
  if (ui_->tabs->currentIndex() == ROI_TAB)
  {
    activateROI();
  }

  QWidget::show();
}


void AutomaticSettings::finished()
{
  disableROI();
  hide();
  emit hidden();
}


void AutomaticSettings::reset()
{
  if (ui_->tabs->currentIndex() == ROI_TAB)
  {
    ui_->roi_surface->resetOverlay();
  }
}



void AutomaticSettings::tabChanged(int index)
{
  if (index == ROI_TAB)
  {
    activateROI();
  }
  else
  {
    disableROI();
  }
}


void AutomaticSettings::activateROI()
{
  Logger::getLogger()->printNormal(this, "Manual ROI window opened. "
                                         "Enabling manual ROI");
  settings_.setValue(SettingsKey::manualROIStatus,         "1");

  emit updateAutomaticSettings();

  // TODO: Once rate control has been moved to automatic side, disable it here
}


void AutomaticSettings::disableROI()
{
  Logger::getLogger()->printNormal(this, "Manual ROI window closed. "
                                         "Disabling manual ROI");
  settings_.setValue(SettingsKey::manualROIStatus,          "0");
  emit updateAutomaticSettings();
}


VideoWidget* AutomaticSettings::getRoiSelfView()
{
  return ui_->roi_surface;
}
