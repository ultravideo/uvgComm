#include "roiarea.h"
#include "ui_roiarea.h"

#include "../gui/videowidget.h"

RoiArea::RoiArea(QWidget *parent) :
  QWidget(parent),
  ui(new Ui::RoiArea)
{
  ui->setupUi(this);
}

RoiArea::~RoiArea()
{
  delete ui;
}
