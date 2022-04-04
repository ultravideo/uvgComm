#include "automaticsettings.h"
#include "ui_automaticsettings.h"

#include "roiarea.h"

AutomaticSettings::AutomaticSettings(QWidget *parent) :
  QDialog(parent),
  ui(new Ui::AutomaticSettings)
{
  ui->setupUi(this);
  QObject::connect(ui->close_button, &QPushButton::clicked,
                   this,             &AutomaticSettings::finished);

  QObject::connect(ui->roi_button, &QPushButton::clicked,
                   this,           &AutomaticSettings::showROI);

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
}
