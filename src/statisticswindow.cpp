#include "statisticswindow.h"
#include "ui_statisticswindow.h"

StatisticsWindow::StatisticsWindow(QWidget *parent) :
QDialog(parent),
ui_(new Ui::StatisticsWindow)
{
  ui_->setupUi(this);
}

StatisticsWindow::~StatisticsWindow()
{
  delete ui_;
}


void StatisticsWindow::closeEvent(QCloseEvent *event)
{}
