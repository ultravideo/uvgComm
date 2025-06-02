#include "demos.h"
#include "ui_demos.h"

#include "settingskeys.h"
#include "logger.h"

enum TabType {
  MAIN_TAB = 0, ROI_TAB = 1
};


const std::vector<std::pair<QString, uint16_t>> OBJECT_TYPES =
{
    {"Person", 0},
    {"Handbag", 26},
    {"Bottle", 39},
    {"Cup", 41},
    {"Bowl", 45},
    {"Orange", 49},
    {"Cell phone", 67},
    {"Book", 73},
    {"Clock", 74},
    {"Toothbrush", 79}
};

Demos::Demos(QWidget *parent):
  QDialog(parent),
  ui_(new Ui::AutomaticSettings),
  settings_(settingsFile, settingsFileFormat),
  previousBitrate_(0),
  lastTabIndex_(0)
{
  ui_->setupUi(this);
  QObject::connect(ui_->close_button, &QPushButton::clicked,
                   this,             &Demos::finished);

  QObject::connect(ui_->reset_button, &QPushButton::clicked,
                   this,             &Demos::reset);

  QObject::connect(ui_->tabs, &QTabWidget::currentChanged,
                   this,      &Demos::tabChanged);

  // the signal in Qt is overloaded (because of deprication) so we need different syntax
  QObject::connect(ui_->roi_qp, QOverload<int>::of(&QSpinBox::valueChanged),
                   this,         &Demos::updateConfigAndReset);

  QObject::connect(ui_->background_qp, QOverload<int>::of(&QSpinBox::valueChanged),
                   this,         &Demos::updateConfigAndReset);

  QObject::connect(ui_->brush_size, QOverload<int>::of(&QSpinBox::valueChanged),
                   this,         &Demos::updateConfig);

  QObject::connect(ui_->show_grid, &QCheckBox::stateChanged,
                   this,         &Demos::updateConfig);

  QObject::connect(ui_->ctu_based, &QCheckBox::stateChanged,
                   this,           &Demos::updateConfigAndReset);
  

  QObject::connect(ui_->buttonGroup, &QButtonGroup ::buttonClicked,
                   this,             &Demos::radioButton);


  settings_.setValue(SettingsKey::manualROIStatus,          "0");

  ui_->roi_surface->enableOverlay(ui_->roi_qp->value(),
                                  ui_->background_qp->value(),
                                  ui_->brush_size->value(),
                                  ui_->show_grid->isChecked(),
                                  !ui_->ctu_based->isChecked(),
                                  getSettingsResolution());

  for(auto& type : OBJECT_TYPES)
  {
    ui_->objectSelection->addItem(type.first);
  }

  settings_.setValue(SettingsKey::roiObject,   "0");

  QObject::connect(ui_->objectSelection, QOverload<int>::of(&QComboBox::activated),
                   this, &Demos::selectObject);

  ui_->objectSelection->hide();
  ui_->ctu_based->hide();

  ui_->brush_size->hide();
  ui_->brush_size_label->hide();

  ui_->roi_surface->disableOverlay();

    settings_.setValue(SettingsKey::roiMode, "off");
}


void Demos::selectObject(int index)
{
  settings_.setValue(SettingsKey::roiObject, OBJECT_TYPES.at(index).second);
  emit updateAutomaticSettings();
}


Demos::~Demos()
{
  delete ui_;
}


void Demos::updateVideoConfig()
{
  updateConfig(0);

  // reset the whole ROI map because changing config benefits from it
  ui_->roi_surface->resetOverlay();

  settings_.setValue(SettingsKey::roiQp, ui_->roi_qp->value());
  settings_.setValue(SettingsKey::backgroundQp, ui_->background_qp->value());

  emit updateAutomaticSettings();
}


void Demos::updateConfigAndReset(int i)
{
  updateVideoConfig();
}


void Demos::radioButton(QAbstractButton * button)
{
    QString roiMode = "off";
  switch (ui_->buttonGroup->id(button))
  {
    case -2:
    {
      Logger::getLogger()->printNormal(this, "Off");
      ui_->ctu_based->hide();
      ui_->brush_size->hide();
      ui_->brush_size_label->hide();
      ui_->objectSelection->hide();
      ui_->roi_surface->disableOverlay();

      roiMode = "off";
      break;
    }
    case -3:
    {
      Logger::getLogger()->printNormal(this, "Manual");
      ui_->ctu_based->show();
      ui_->brush_size->show();
      ui_->brush_size_label->show();
      ui_->objectSelection->hide();

      roiMode = "manual";
      updateConfig(1);
      ui_->roi_surface->resetOverlay();
      break;
    }
    case -4:
    {
      Logger::getLogger()->printNormal(this, "Model");
      ui_->ctu_based->hide();
      ui_->brush_size->hide();
      ui_->brush_size_label->hide();
      ui_->objectSelection->show();
      roiMode = "auto";

      updateConfig(1);
      ui_->roi_surface->resetOverlay();
      break;
    }
    default:
    {
      break;
    }
  }

  settings_.setValue(SettingsKey::roiMode, roiMode);
  emit updateAutomaticSettings();
}


void Demos::updateConfig(int i)
{
  ui_->roi_surface->enableOverlay(ui_->roi_qp->value(),
                                  ui_->background_qp->value(),
                                  ui_->brush_size->value(),
                                  ui_->show_grid->isChecked(),
                                  !ui_->ctu_based->isChecked(),
                                  getSettingsResolution());
}


void Demos::show()
{
  Logger::getLogger()->printNormal(this, "Showing media settings");

  if (ui_->tabs->currentIndex() == ROI_TAB)
  {
    activateROI();
  }

  QWidget::show();
}


void Demos::closeEvent(QCloseEvent *event)
{
  Logger::getLogger()->printNormal(this, "Closing media settings");

  finished();
  QWidget::closeEvent(event);
}


void Demos::finished()
{
  if (ui_->tabs->currentIndex() == ROI_TAB)
  {
    disableROI();
  }

  hide();
  emit hidden();
}


void Demos::reset()
{
  if (ui_->tabs->currentIndex() == ROI_TAB)
  {
    ui_->roi_surface->resetOverlay();
  }
}


void Demos::tabChanged(int index)
{
  // disable the last tab
  if (lastTabIndex_ == ROI_TAB)
  {
    disableROI();
  }

  // enable the new tab
  if (index == ROI_TAB)
  {
    activateROI();
  }

  lastTabIndex_ = index;
}


void Demos::activateROI()
{
  Logger::getLogger()->printNormal(this, "Manual ROI window opened. "
                                         "Enabling manual ROI");

  previousBitrate_ = settings_.value(SettingsKey::videoBitrate).toInt();
  if (previousBitrate_ != 0)
  {
    // bitrate must be disabled for ROI
    settings_.setValue(SettingsKey::videoBitrate,          "0");

    emit updateVideoSettings();
  }

  settings_.setValue(SettingsKey::manualROIStatus,         "1");
  emit updateAutomaticSettings();
}


void Demos::disableROI()
{
  Logger::getLogger()->printNormal(this, "Manual ROI window closed. "
                                         "Disabling manual ROI");
  settings_.setValue(SettingsKey::manualROIStatus,          "0");
  emit updateAutomaticSettings();

  if (previousBitrate_ != 0) // only set bitrate if we had to disable it
  {
    // return bitrate to previous value
    settings_.setValue(SettingsKey::videoBitrate, previousBitrate_);

    emit updateVideoSettings();
  }
}


VideoWidget* Demos::getRoiSelfView()
{
  return ui_->roi_surface;
}


QSize Demos::getSettingsResolution()
{
  QSize resolution;
  resolution.setWidth(settings_.value(SettingsKey::videoResolutionWidth).toInt());
  resolution.setHeight(settings_.value(SettingsKey::videoResolutionHeight).toInt());

  return resolution;
}
