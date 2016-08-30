#include "statisticswindow.h"
#include "ui_statisticswindow.h"

StatisticsWindow::StatisticsWindow(QWidget *parent) :
QDialog(parent),
StatisticsInterface(),
ui_(new Ui::StatisticsWindow)
{
  ui_->setupUi(this);
  ui_->participantTable->setColumnCount(2);
  ui_->participantTable->setHorizontalHeaderItem(0, new QTableWidgetItem(QString("IP")));
  ui_->participantTable->setHorizontalHeaderItem(1, new QTableWidgetItem(QString("Port")));
}

StatisticsWindow::~StatisticsWindow()
{
  delete ui_;
}


void StatisticsWindow::closeEvent(QCloseEvent *event)
{}

void StatisticsWindow::addNextInterface(StatisticsInterface* next)
{}

void StatisticsWindow::videoInfo(double framerate, QSize resolution)
{}

void StatisticsWindow::audioInfo(uint32_t sampleRate)
{}

void StatisticsWindow::addParticipant(QString ip, QString port)
{
  ui_->participantTable->insertRow(ui_->participantTable->rowCount());
  ui_->participantTable->setItem(ui_->participantTable->rowCount() -1, 0, new QTableWidgetItem(ip));
  ui_->participantTable->setItem(ui_->participantTable->rowCount() -1, 1, new QTableWidgetItem(port));
}

void StatisticsWindow::delayTime(QString type, struct timeval)
{}

void StatisticsWindow::addSendPacket(uint16_t size)
{}

void StatisticsWindow::updateBufferStatus(QString filter, uint16_t buffersize)
{}
