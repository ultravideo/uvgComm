#include "statisticswindow.h"
#include "ui_statisticswindow.h"

#include <QCloseEvent>

StatisticsWindow::StatisticsWindow(QWidget *parent) :
QDialog(parent),
StatisticsInterface(),
ui_(new Ui::StatisticsWindow),
  framerate_(0),
  bitrateCounter_(0),
  bitrate_(0),
  sendPacketCount_(0),
  transferredData_(0),
  receivePacketCount_(0),
  receivedData_(0),
  packetsDropped_(0)
{
  ui_->setupUi(this);
  ui_->participantTable->setColumnCount(2); // more columns can be added later
  ui_->participantTable->setHorizontalHeaderItem(0, new QTableWidgetItem(QString("IP")));
  ui_->participantTable->setHorizontalHeaderItem(1, new QTableWidgetItem(QString("Port")));
}

StatisticsWindow::~StatisticsWindow()
{
  delete ui_;
}

void StatisticsWindow::closeEvent(QCloseEvent *event)
{
  accept();
}

void StatisticsWindow::addNextInterface(StatisticsInterface* next)
{}

void StatisticsWindow::videoInfo(double framerate, QSize resolution)
{
  framerate_ = framerate;
  ui_->framerate_value->setText( QString::number(framerate)+"fps");
  ui_->resolution_value->setText( QString::number(resolution.width()) + "x"
                          + QString::number(resolution.height()));
}

void StatisticsWindow::audioInfo(uint32_t sampleRate)
{

}

void StatisticsWindow::addParticipant(QString ip, QString port)
{
  ui_->participantTable->insertRow(ui_->participantTable->rowCount());
  // add cells to table
  ui_->participantTable->setItem(ui_->participantTable->rowCount() -1, 0, new QTableWidgetItem(ip));
  ui_->participantTable->setItem(ui_->participantTable->rowCount() -1, 1, new QTableWidgetItem(port));
}

void StatisticsWindow::delayTime(QString type, uint32_t delay)
{
  if(type == "video")
  {
    sendMutex_.lock();
    ui_->encode_delay_value->setText( QString::number(delay) + "ms. (not precise)" );
    sendMutex_.unlock();
  }
}
void StatisticsWindow::addEncodedVideo(uint16_t size)
{
  // TODO: assumes constant flow of frames at framerate
  if(framerate_ == 0)
    ui_->video_bitrate_value->setText( QString::number(size) + "bytes" );
  else
  {
    if(bitrateCounter_ == framerate_)
    {
      bitrateCounter_ = 0;
      bitrate_ = 0;
    }
    ++bitrateCounter_;
    bitrate_ += size;
  }
}

void StatisticsWindow::addSendPacket(uint16_t size)
{
  sendMutex_.lock();
  ++sendPacketCount_;
  transferredData_ += size;
  sendMutex_.unlock();
}

void StatisticsWindow::addReceivePacket(uint16_t size)
{
  receiveMutex_.lock();
  ++receivePacketCount_;
  receivedData_ += size;
  receiveMutex_.unlock();
}

void StatisticsWindow::updateBufferStatus(QString filter, uint16_t buffersize)
{
  bufferMutex_.lock();
  if(buffers_.find(filter) == buffers_.end() || buffers_[filter] != buffersize)
  {
    dirtyBuffers_ = true;
    buffers_[filter] = buffersize;
  }
  bufferMutex_.unlock();
}

void StatisticsWindow::packetDropped()
{
  ++packetsDropped_;
}

void StatisticsWindow::paintEvent(QPaintEvent *event)
{
  if(bitrateCounter_ == framerate_)
  {
    ui_->video_bitrate_value->setText(QString::number(bitrate_*8/1000) + "kbit/s. (if correct framerate)" );
  }
  ui_->packets_sent_value->setText( QString::number(sendPacketCount_));
  ui_->data_sent_value->setText( QString::number(transferredData_));
  ui_->packets_received_value->setText( QString::number(receivePacketCount_));
  ui_->data_received_value->setText( QString::number(receivedData_));
  ui_->packets_skipped_value->setText(QString::number(packetsDropped_));

  if(dirtyBuffers_)
  {
    bufferMutex_.lock();
    ui_->buffer_sizes_value->setText( QString::number(totalBuffers()));
    dirtyBuffers_ = false;
    bufferMutex_.unlock();
  }
}

uint32_t StatisticsWindow::totalBuffers()
{
  uint32_t total = 0;
  for(auto& it : buffers_)
  {
    total += it.second;
  }
  return total;
}
