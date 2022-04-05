#include "roiarea.h"
#include "ui_roiarea.h"

#include "../gui/videowidget.h"

RoiArea::RoiArea(QWidget *parent) :
  QWidget(parent),
  ui_(new Ui::RoiArea)
{
  ui_->setupUi(this);
  ui_->frame->enableOverlay();
}


RoiArea::~RoiArea()
{
  delete ui_;
}


VideoWidget* RoiArea::getSelfVideoWidget()
{
  return ui_->frame;
}
