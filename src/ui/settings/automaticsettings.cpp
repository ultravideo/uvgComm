#include "automaticsettings.h"
#include "ui_automaticsettings.h"


AutomaticSettings::AutomaticSettings(QWidget *parent) :
  QDialog(parent),
  ui(new Ui::AutomaticSettings)
{
  ui->setupUi(this);
  QObject::connect(ui->close_button, &QPushButton::clicked,
                   this,             &AutomaticSettings::finished);
}


AutomaticSettings::~AutomaticSettings()
{
  delete ui;
}


void AutomaticSettings::finished()
{
  hide();
  emit hidden();
}
