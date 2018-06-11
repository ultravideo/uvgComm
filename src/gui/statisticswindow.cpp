#include "statisticswindow.h"

#include "ui_statisticswindow.h"

#include <QCloseEvent>
#include <QDateTime>
#include <QDebug>

const int BUFFERSIZE = 65536;

StatisticsWindow::StatisticsWindow(QWidget *parent) :
QDialog(parent),
StatisticsInterface(),
ui_(new Ui::StatisticsWindow),
  framerate_(0),
  videoIndex_(0), // ringbuffer index
  videoPackets_(BUFFERSIZE,0), // ringbuffer
  audioIndex_(0), // ringbuffer index
  audioPackets_(BUFFERSIZE,0), // ringbuffer
  sendPacketCount_(0),
  transferredData_(0),
  receivePacketCount_(0),
  receivedData_(0),
  packetsDropped_(0),
  lastVideoBitrate_(0),
  lastAudioBitrate_(0),
  lastVideoFrameRate_(0.0f),
  lastAudioFrameRate_(0.0f)
{
  ui_->setupUi(this);
  ui_->participantTable->setColumnCount(4); // more columns can be added later
  ui_->participantTable->setHorizontalHeaderItem(0, new QTableWidgetItem(QString("IP")));
  ui_->participantTable->setHorizontalHeaderItem(1, new QTableWidgetItem(QString("AudioPort")));
  ui_->participantTable->setHorizontalHeaderItem(1, new QTableWidgetItem(QString("VideoPort")));
  ui_->participantTable->setHorizontalHeaderItem(2, new QTableWidgetItem(QString("Audio delay")));
  ui_->participantTable->setHorizontalHeaderItem(3, new QTableWidgetItem(QString("Video delay")));

  ui_->filterTable->setColumnCount(3); // more columns can be added later
  ui_->filterTable->setHorizontalHeaderItem(0, new QTableWidgetItem(QString("Filter")));
  ui_->filterTable->setHorizontalHeaderItem(1, new QTableWidgetItem(QString("TID")));
  ui_->filterTable->setHorizontalHeaderItem(2, new QTableWidgetItem(QString("Buffer Size")));

  ui_->filterTable->setColumnWidth(0, 240);
}

StatisticsWindow::~StatisticsWindow()
{
  delete ui_;
}

void StatisticsWindow::closeEvent(QCloseEvent *event)
{
  Q_UNUSED(event)
  accept();
}

void StatisticsWindow::addNextInterface(StatisticsInterface* next)
{
  Q_UNUSED(next)
  Q_ASSERT(false && "NOT IMPLEMENTED");
  qWarning() << "WARNING: addNextInterface has not been implemented in stat window";
}

void StatisticsWindow::videoInfo(double framerate, QSize resolution)
{
  framerate_ = framerate;
  ui_->framerate_value->setText( QString::number(framerate)+" fps");
  ui_->resolution_value->setText( QString::number(resolution.width()) + "x"
                          + QString::number(resolution.height()));
}

void StatisticsWindow::audioInfo(uint32_t sampleRate, uint16_t channelCount)
{
  //ui_->a_framerate_value->setText(QString::number(framerate)+"fps");
  if(sampleRate == 0 || sampleRate == 4294967295)
  {
    ui_->channels_value->setText("No Audio");
    ui_->sample_rate_value->setText("No Audio");
  }
  else
  {
    ui_->channels_value->setText(QString::number(channelCount));
    ui_->sample_rate_value->setText(QString::number(sampleRate) + " Hz");
  }
}

void StatisticsWindow::addParticipant(QString ip, QString audioPort, QString videoPort)
{
  initMutex_.lock();
  ui_->participantTable->insertRow(ui_->participantTable->rowCount());
  // add cells to table
  ui_->participantTable->setItem(ui_->participantTable->rowCount() -1, 0, new QTableWidgetItem(ip));
  ui_->participantTable->setItem(ui_->participantTable->rowCount() -1, 1, new QTableWidgetItem(audioPort));
  ui_->participantTable->setItem(ui_->participantTable->rowCount() -1, 2, new QTableWidgetItem(videoPort));
  ui_->participantTable->setItem(ui_->participantTable->rowCount() -1, 3, new QTableWidgetItem("- ms"));
  ui_->participantTable->setItem(ui_->participantTable->rowCount() -1, 4, new QTableWidgetItem("- ms"));

  delays_.push_back({0, 0,true});
  initMutex_.unlock();
}

void StatisticsWindow::addFilterTID(QString filter, uint64_t TID)
{
  initMutex_.lock();
  ui_->filterTable->insertRow(ui_->filterTable->rowCount());
  ui_->filterTable->setItem(ui_->filterTable->rowCount() -1, 0, new QTableWidgetItem(filter));
  ui_->filterTable->setItem(ui_->filterTable->rowCount() -1, 1, new QTableWidgetItem(QString::number(TID)));
  initMutex_.unlock();
}

void StatisticsWindow::removeParticipant(QString ip)
{
  initMutex_.lock();
  for(int i = 0; i < ui_->participantTable->rowCount(); ++i)
  {
    if(ui_->participantTable->item(i, 0)->text() == ip )
    {
      ui_->participantTable->removeRow(i);
      delays_.at(i).active = false;
    }
  }
  initMutex_.unlock();
}

void StatisticsWindow::sendDelay(QString type, uint32_t delay)
{
  if(type == "video" || type == "Video")
  {
    sendMutex_.lock();
    ui_->encode_delay_value->setText( QString::number(delay) + " ms." );
    sendMutex_.unlock();
  }
  else if(type == "audio" || type == "Audio")
  {
    sendMutex_.lock();
    ui_->audio_delay_value->setText( QString::number(delay) + " ms." );
    sendMutex_.unlock();
  }
}

void StatisticsWindow::receiveDelay(uint32_t peer, QString type, int32_t delay)
{
  if(type == "video" || type == "Video")
  {
    delays_.at(peer - 1).video = delay;
  }
  else if(type == "audio" || type == "Audio")
  {
    delays_.at(peer - 1).audio = delay;
  }
}

void StatisticsWindow::addEncodedPacket(QString type, uint16_t size)
{
  PacketInfo *packet = new PacketInfo{QDateTime::currentMSecsSinceEpoch(), size};

  if(type == "video" || type == "Video")
  {
    if(videoPackets_[videoIndex_%BUFFERSIZE])
    {
      delete videoPackets_.at(videoIndex_%BUFFERSIZE);
    }

    videoPackets_[videoIndex_%BUFFERSIZE] = packet;

    ++videoIndex_;
  }
  else if(type == "audio" || type == "Audio")
  {
    if(audioPackets_[audioIndex_%BUFFERSIZE])
    {
      delete audioPackets_.at(audioIndex_%BUFFERSIZE);
    }

    audioPackets_[audioIndex_%BUFFERSIZE] = packet;

    ++audioIndex_;
  }
}

uint32_t StatisticsWindow::bitrate(std::vector<PacketInfo*>& packets, uint32_t index, float& framerate)
{
  if(index == 0)
    return 0;

  uint32_t currentTime = QDateTime::currentMSecsSinceEpoch();
  uint32_t timeInterval = 0;
  uint32_t bitrate = 0;
  uint32_t bitrateInterval = 5000;
  framerate = 0;

  uint32_t i = index - 1;

  PacketInfo* p = packets[i%BUFFERSIZE];

  while(p && timeInterval < bitrateInterval)
  {
    bitrate += p->size;
    ++framerate;
    if(i != 0)
      --i;
    else
      i = BUFFERSIZE - 1;

    timeInterval = currentTime - p->timestamp;
    p = packets[i%BUFFERSIZE];
  }

  //qDebug() << "Bitrate:" << bitrate << "timeInterval:"  << timeInterval;
  if(timeInterval)
  {
    framerate = 1000*framerate/timeInterval;
    return 8*bitrate/(timeInterval);
  }
  return 0;
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

void StatisticsWindow::updateBufferStatus(QString filter, uint16_t buffersize, uint16_t maxBufferSize)
{
  bufferMutex_.lock();
  if(buffers_.find(filter) == buffers_.end() || buffers_[filter] != buffersize)
  {
    dirtyBuffers_ = true;
    buffers_[filter] = buffersize;

    QList<QTableWidgetItem*> filter_name = ui_->filterTable->findItems(filter, Qt::MatchExactly);
    if(filter_name.size() == 1)
    {
      ui_->filterTable->setItem(filter_name.at(0)->row(), 2,
                                new QTableWidgetItem(QString::number(buffersize) + "/" + QString::number(maxBufferSize)));
    }
  }
  bufferMutex_.unlock();
}

void StatisticsWindow::packetDropped()
{
  ++packetsDropped_;
}

void StatisticsWindow::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event)

  if(videoIndex_%15 == 0)
  {
    lastVideoBitrate_ = bitrate(videoPackets_, videoIndex_, lastVideoFrameRate_);
    ui_->video_bitrate_value->setText
      ( QString::number(lastVideoBitrate_) + " kbit/s" );

    ui_->encoded_framerate_value->setText
      ( QString::number(lastVideoFrameRate_) + " fps" );

    uint index = 0;
    for(Delays d : delays_)
    {
      // if this participant has not yet been removed
      // also tells whether the slot for this participant exists
      if(d.active)
      {
        ui_->participantTable->setItem(index, 3, new QTableWidgetItem( QString::number(d.audio) + " ms"));
        ui_->participantTable->setItem(index, 4, new QTableWidgetItem( QString::number(d.video) + " ms"));
        ++index;
      }
    }
  }

  if(audioIndex_%20 == 0)
  {
    lastAudioBitrate_ = bitrate(audioPackets_, audioIndex_, lastAudioFrameRate_);
    ui_->audio_bitrate_value->setText
      ( QString::number(lastAudioBitrate_) + " kbit/s" );
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
