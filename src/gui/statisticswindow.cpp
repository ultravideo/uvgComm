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
  lastAudioFrameRate_(0.0f),
  audioEncDelay_(0),
  videoEncDelay_(0),
  guiTimer_(),
  lastDrawTime_(0),
  guiFrequency_(1000),
  lastTabIndex_(254) // an invalid value so we will update the tab immediately
{
  ui_->setupUi(this);
  ui_->participantTable->setColumnCount(6);
  ui_->participantTable->setHorizontalHeaderItem(0, new QTableWidgetItem(QString("IP")));
  ui_->participantTable->setHorizontalHeaderItem(1, new QTableWidgetItem(QString("AudioPort")));
  ui_->participantTable->setHorizontalHeaderItem(2, new QTableWidgetItem(QString("VideoPort")));
  ui_->participantTable->setHorizontalHeaderItem(3, new QTableWidgetItem(QString("Audio delay")));
  ui_->participantTable->setHorizontalHeaderItem(4, new QTableWidgetItem(QString("Video delay")));
  ui_->participantTable->setHorizontalHeaderItem(5, new QTableWidgetItem(QString("Video fps")));

  ui_->filterTable->setColumnCount(4);
  ui_->filterTable->setHorizontalHeaderItem(0, new QTableWidgetItem(QString("Filter")));
  ui_->filterTable->setHorizontalHeaderItem(1, new QTableWidgetItem(QString("TID")));
  ui_->filterTable->setHorizontalHeaderItem(2, new QTableWidgetItem(QString("Buffer Size")));
  ui_->filterTable->setHorizontalHeaderItem(3, new QTableWidgetItem(QString("Dropped")));

  ui_->filterTable->setColumnWidth(0, 240);

  guiTimer_.start();
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
  // done only once, so setting ui directly is ok.
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
  ui_->participantTable->setItem(ui_->participantTable->rowCount() -1, 5, new QTableWidgetItem("-"));

  peers_.push_back({0, std::vector<PacketInfo*>(BUFFERSIZE,0), 0, 0,true});
  initMutex_.unlock();
}

void StatisticsWindow::addFilter(QString filter, uint64_t TID)
{
  if(buffers_.find(filter) == buffers_.end())
  {
    bufferMutex_.lock();
    buffers_[filter] = FilterStatus{0,QString::number(TID),0,0};
    bufferMutex_.unlock();
  }
  else
  {
    return;
  }

  initMutex_.lock();
  ui_->filterTable->insertRow(ui_->filterTable->rowCount());
  ui_->filterTable->setItem(ui_->filterTable->rowCount() -1, 0, new QTableWidgetItem(filter));
  ui_->filterTable->setItem(ui_->filterTable->rowCount() -1, 1, new QTableWidgetItem(QString::number(TID)));
  ui_->filterTable->setItem(ui_->filterTable->rowCount() -1, 3, new QTableWidgetItem(QString::number(0)));
  initMutex_.unlock();
}

void StatisticsWindow::removeFilter(QString filter)
{
  initMutex_.lock();
  ui_->filterTable->removeRow(ui_->filterTable->rowCount() - 1);
  initMutex_.unlock();

  buffers_.erase(filter);
}

// TODO: Does not work when callling ourselves with same address.
void StatisticsWindow::removeParticipant(QString ip)
{
  initMutex_.lock();
  for(int i = 0; i < ui_->participantTable->rowCount(); ++i)
  {
    if(ui_->participantTable->item(i, 0)->text() == ip )
    {
      ui_->participantTable->removeRow(i);
      peers_.at(i).active = false;
    }
  }
  initMutex_.unlock();
}

void StatisticsWindow::sendDelay(QString type, uint32_t delay)
{
  if(type == "video" || type == "Video")
  {
    videoEncDelay_ = delay;
  }
  else if(type == "audio" || type == "Audio")
  {
    audioEncDelay_ = delay;
  }
}

void StatisticsWindow::receiveDelay(uint32_t peer, QString type, int32_t delay)
{
  if(type == "video" || type == "Video")
  {
    peers_.at(peer - 1).videoDelay = delay;
  }
  else if(type == "audio" || type == "Audio")
  {
    peers_.at(peer - 1).audioDelay = delay;
  }
}

void StatisticsWindow::presentPackage(uint32_t peer, QString type)
{
  Q_ASSERT(peers_.size() >= peer);
  if(peer <= peers_.size())
  {
    if(type == "video" || type == "Video")
    {
      updateFramerateBuffer(peers_.at(peer - 1).videoPackets, peers_.at(peer - 1).videoIndex, 0);
    }
  }
}

void StatisticsWindow::addEncodedPacket(QString type, uint16_t size)
{
  if(type == "video" || type == "Video")
  {
    updateFramerateBuffer(videoPackets_, videoIndex_, size);
  }
  else if(type == "audio" || type == "Audio")
  {
    updateFramerateBuffer(audioPackets_, audioIndex_, size);
  }
}

void StatisticsWindow::updateFramerateBuffer(std::vector<PacketInfo*>& packets, uint32_t& index, uint32_t size)
{
  if(packets[index%BUFFERSIZE])
  {
    delete packets.at(index%BUFFERSIZE);
  }

  packets[index%BUFFERSIZE] = new PacketInfo{QDateTime::currentMSecsSinceEpoch(), size};
  ++index;
}

uint32_t StatisticsWindow::bitrate(std::vector<PacketInfo*>& packets, uint32_t index, float& framerate)
{
  if(index == 0)
    return 0;

  int64_t timeInterval = 0;
  int64_t bitrate = 0;
  int64_t bitrateInterval = 5000; // how long time in msecs we measure
  uint16_t frames = 0;
  uint32_t currentTs = 0;
  uint32_t previousTs = index - 2;
  if(index == 0)
  {
    currentTs = BUFFERSIZE - 1;
    previousTs = BUFFERSIZE - 2;
  }
  else if (index == 1)
  {
    currentTs = index - 1;
    previousTs = BUFFERSIZE - 1;
  }
  else
  {
    currentTs = index - 1;
    previousTs = index - 2;
  }

  while(packets[previousTs%BUFFERSIZE] && timeInterval < bitrateInterval)
  {
    timeInterval += packets[currentTs%BUFFERSIZE]->timestamp - packets[previousTs%BUFFERSIZE]->timestamp;

    bitrate += packets[currentTs%BUFFERSIZE]->size;
    ++frames;
    currentTs = previousTs;
    if(previousTs != 0)
    {
      --previousTs;
    }
    else
    {
      previousTs = BUFFERSIZE - 1;
    }
  }

  //qDebug() << "Bitrate:" << bitrate << "timeInterval:"  << timeInterval;
  if(timeInterval)
  {
    framerate = 1000*(float)frames/timeInterval;
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
  if(buffers_.find(filter) != buffers_.end())
  {
    if(buffers_[filter].bufferStatus != buffersize || buffers_[filter].bufferSize != maxBufferSize)
    {
      dirtyBuffers_ = true;
      buffers_[filter].bufferStatus = buffersize;
      buffers_[filter].bufferSize = maxBufferSize;
    }
  }
  else
  {
    qDebug() << "Couldn't find correct filter for buffer status:" << filter;
  }
  bufferMutex_.unlock();
}

void StatisticsWindow::packetDropped(QString filter)
{
  ++packetsDropped_;
  if(buffers_.find(filter) != buffers_.end())
  {
    ++buffers_[filter].dropped;
    dirtyBuffers_ = true;
  }
  else
  {
    qDebug() << "Couldn't find correct filter for dropped packet:" << filter;
  }
}

void StatisticsWindow::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event)
  if(lastTabIndex_ != ui_->Statistics_tabs->currentIndex() || lastDrawTime_ + guiFrequency_ < guiTimer_.elapsed())
  {
    // no need to catch up if we are falling behind, instead just reset the clock
    lastDrawTime_ = guiTimer_.elapsed();

    switch(ui_->Statistics_tabs->currentIndex())
    {
    case 0:
    {
      uint index = 0;
      for(PeerInfo d : peers_)
      {
        // if this participant has not yet been removed
        // also tells whether the slot for this participant exists
        if(d.active)
        {
          ui_->participantTable->setItem(index, 3, new QTableWidgetItem( QString::number(d.audioDelay) + " ms"));
          ui_->participantTable->setItem(index, 4, new QTableWidgetItem( QString::number(d.videoDelay) + " ms"));
          float framerate = 0;
          uint32_t videoBitrate = bitrate(d.videoPackets, d.videoIndex, framerate);
          ui_->participantTable->setItem(index, 5, new QTableWidgetItem( QString::number(framerate)));

          ++index;
        }
      }
      break;
    }
    case 1:
    {
      ui_->packets_sent_value->setText( QString::number(sendPacketCount_));
      ui_->data_sent_value->setText( QString::number(transferredData_));
      ui_->packets_received_value->setText( QString::number(receivePacketCount_));
      ui_->data_received_value->setText( QString::number(receivedData_));
      ui_->packets_skipped_value->setText(QString::number(packetsDropped_));
      break;
    case 2:
        lastVideoBitrate_ = bitrate(videoPackets_, videoIndex_, lastVideoFrameRate_);
        ui_->video_bitrate_value->setText
            ( QString::number(lastVideoBitrate_) + " kbit/s" );

        ui_->encoded_framerate_value->setText
            ( QString::number(lastVideoFrameRate_) + " fps" );


        ui_->encode_delay_value->setText( QString::number(videoEncDelay_) + " ms." );
        ui_->audio_delay_value->setText( QString::number(audioEncDelay_) + " ms." );

        lastAudioBitrate_ = bitrate(audioPackets_, audioIndex_, lastAudioFrameRate_);
        ui_->audio_bitrate_value->setText
            ( QString::number(lastAudioBitrate_) + " kbit/s" );
        break;
      }
    case 3:
    {

      if(dirtyBuffers_)
      {
        bufferMutex_.lock();

        uint32_t totalBuffers = 0;
        uint32_t row = 0;
        for(auto& it : buffers_)
        {
          totalBuffers += it.second.bufferStatus;
          ui_->filterTable->setItem(row, 0,new QTableWidgetItem(it.first));
          ui_->filterTable->setItem(row, 1,new QTableWidgetItem(it.second.TID));
          ui_->filterTable->setItem(row, 2,new QTableWidgetItem(QString::number(it.second.bufferStatus) +
                                                                "/" + QString::number(it.second.bufferSize)));
          ui_->filterTable->setItem(row, 3,new QTableWidgetItem(QString::number(it.second.dropped)));
          ++row;
        }

        ui_->buffer_sizes_value->setText(QString::number(totalBuffers));
        dirtyBuffers_ = false;
        bufferMutex_.unlock();
      }
      break;
    }
    default:
    {
      break;
    }
    }
    lastTabIndex_ = ui_->Statistics_tabs->currentIndex();
  }
}
