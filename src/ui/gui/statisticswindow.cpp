#include "statisticswindow.h"

#include "ui_statisticswindow.h"

#include "common.h"

#include <QCloseEvent>
#include <QDateTime>
#include <QDebug>


#ifdef QT_CHARTS_LIB
#include <QtCharts/QValueAxis>
#include <QtCharts/QAbstractAxis>
#endif

const int BUFFERSIZE = 65536;

const int GRAPHSIZE = 60;

const int FPSPRECISION = 4;

const int UPDATEFREQUENCY = 1000;

enum TabType {
  SIP_TAB = 0, PARTICIPANT_TAB = 1, NETWORK_TAB = 2, MEDIA_TAB = 3, FILTER_TAB = 4
};


StatisticsWindow::StatisticsWindow(QWidget *parent) :
QDialog(parent),
StatisticsInterface(),
ui_(new Ui::StatisticsWindow),
  framerate_(0),
  videoIndex_(0), // ringbuffer index
  videoPackets_(BUFFERSIZE,nullptr), // ringbuffer
  audioIndex_(0), // ringbuffer index
  audioPackets_(BUFFERSIZE,nullptr), // ringbuffer
  sendPacketCount_(0),
  transferredData_(0),
  receivePacketCount_(0),
  receivedData_(0),
  packetsDropped_(0),
  audioEncDelay_(0),
  videoEncDelay_(0),
  guiTimer_(),
  guiUpdates_(0),
  lastTabIndex_(254) // an invalid value so we will update the tab immediately
{
  ui_->setupUi(this);

  // init headers of participant table
  participantMutex_.lock();
  ui_->participantTable->setColumnCount(6);
  ui_->participantTable->setHorizontalHeaderItem(0, new QTableWidgetItem(QString("IP")));
  ui_->participantTable->setHorizontalHeaderItem(1, new QTableWidgetItem(QString("AudioPort")));
  ui_->participantTable->setHorizontalHeaderItem(2, new QTableWidgetItem(QString("VideoPort")));
  ui_->participantTable->setHorizontalHeaderItem(3, new QTableWidgetItem(QString("Audio delay")));
  ui_->participantTable->setHorizontalHeaderItem(4, new QTableWidgetItem(QString("Video delay")));
  ui_->participantTable->setHorizontalHeaderItem(5, new QTableWidgetItem(QString("Video fps")));
  participantMutex_.unlock();

  // init headers of filter table
  filterTableMutex_.lock();
  ui_->filterTable->setColumnCount(4);
  ui_->filterTable->setHorizontalHeaderItem(0, new QTableWidgetItem(QString("Filter")));
  ui_->filterTable->setHorizontalHeaderItem(1, new QTableWidgetItem(QString("TID")));
  ui_->filterTable->setHorizontalHeaderItem(2, new QTableWidgetItem(QString("Buffer Size")));
  ui_->filterTable->setHorizontalHeaderItem(3, new QTableWidgetItem(QString("Dropped")));

  ui_->filterTable->setColumnWidth(0, 240);
  filterTableMutex_.unlock();



#ifndef QT_CHARTS_LIB
  ui_->fps_graph->hide();
#endif
}


StatisticsWindow::~StatisticsWindow()
{
  delete ui_;
}


void StatisticsWindow::showEvent(QShowEvent * event)
{
  Q_UNUSED(event)
  // start refresh timer
  guiTimer_.start();
  guiUpdates_ = 0;
}

void StatisticsWindow::closeEvent(QCloseEvent *event)
{
  Q_UNUSED(event)
  accept();
}


void StatisticsWindow::addNextInterface(StatisticsInterface* next)
{
  Q_UNUSED(next)
  printUnimplemented(this, "addNextInterface has not been implemented in stat window");
}


void StatisticsWindow::videoInfo(double framerate, QSize resolution)
{
  // done only once, so setting ui directly is ok.
  framerate_ = framerate;
  ui_->framerate_value->setText( QString::number(framerate, 'g', FPSPRECISION)+" fps");
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


void StatisticsWindow::addParticipant(uint32_t sessionID, QString ip,
                                      QString audioPort, QString videoPort)
{
  participantMutex_.lock();
  ui_->participantTable->insertRow(ui_->participantTable->rowCount());
  // add cells to table
  ui_->participantTable->setItem(ui_->participantTable->rowCount() -1, 0,
                                 new QTableWidgetItem(ip));
  ui_->participantTable->setItem(ui_->participantTable->rowCount() -1, 1,
                                 new QTableWidgetItem(audioPort));
  ui_->participantTable->setItem(ui_->participantTable->rowCount() -1, 2,
                                 new QTableWidgetItem(videoPort));
  ui_->participantTable->setItem(ui_->participantTable->rowCount() -1, 3,
                                 new QTableWidgetItem("- ms"));
  ui_->participantTable->setItem(ui_->participantTable->rowCount() -1, 4,
                                 new QTableWidgetItem("- ms"));
  ui_->participantTable->setItem(ui_->participantTable->rowCount() -1, 5,
                                 new QTableWidgetItem("-"));

  peers_[sessionID] = {0, std::vector<PacketInfo*>(BUFFERSIZE, nullptr),
                       0, 0, ui_->participantTable->rowCount() - 1};

  participantMutex_.unlock();
}


void StatisticsWindow::addFilter(QString filter, uint64_t TID)
{
  int rowIndex = 0;
  filterTableMutex_.lock();
  ui_->filterTable->insertRow(ui_->filterTable->rowCount());
  rowIndex = ui_->filterTable->rowCount() - 1;
  ui_->filterTable->setItem(ui_->filterTable->rowCount() -1, 0, new QTableWidgetItem(filter));
  ui_->filterTable->setItem(ui_->filterTable->rowCount() -1, 1, new QTableWidgetItem(QString::number(TID)));
  ui_->filterTable->setItem(ui_->filterTable->rowCount() -1, 3, new QTableWidgetItem(QString::number(0)));
  filterTableMutex_.unlock();

  bufferMutex_.lock();
  if(buffers_.find(filter) == buffers_.end())
  {
    buffers_[filter] = FilterStatus{0,QString::number(TID), 0, 0, rowIndex};
  }
  else
  {
    bufferMutex_.unlock();
    printProgramWarning(this, "Tried to add a new filter with same name as previous");
    return;
  }
  bufferMutex_.unlock();
}


void StatisticsWindow::removeFilter(QString filter)
{
  bufferMutex_.lock();
  if (buffers_.find(filter) == buffers_.end())
  {
    bufferMutex_.unlock();
    printProgramWarning(this, "Tried to remove non-existing filter.", {"Name"}, {filter});
    return;
  }
  bufferMutex_.unlock();

  filterTableMutex_.lock();
  if (ui_->filterTable->rowCount() <= buffers_[filter].tableIndex)
  {
    printProgramWarning(this, "Filter doesn't exist in filter table when removing.", {"Name"}, {filter});
    filterTableMutex_.unlock();
    return;
  }

  ui_->filterTable->removeRow(buffers_[filter].tableIndex);
  filterTableMutex_.unlock();

  bufferMutex_.lock();
  // adjust all existing indexes
  for (auto& buffer: buffers_)
  {
    if (buffers_[filter].tableIndex < buffer.second.tableIndex)
    {
      buffer.second.tableIndex -= 1;
    }
  }

  buffers_.erase(filter);
  bufferMutex_.unlock();
}


void StatisticsWindow::removeParticipant(uint32_t sessionID)
{
  // check that peer exists
  if (peers_.find(sessionID) == peers_.end())
  {
    printProgramWarning(this, "Tried to remove a participant that doesn't exist");
    return;
  }

  participantMutex_.lock();

  int participantIndex = peers_[sessionID].participantIndex;

  // check that index points to a valid row
  if (ui_->participantTable->rowCount() <= participantIndex)
  {
    participantMutex_.unlock();
    printProgramWarning(this, "Missing participant row for participant");
    return;
  }

  // remove row from UI
  ui_->participantTable->removeRow(participantIndex);

  // adjust the rest of the peers if needed
  for (auto &peer : peers_)
  {
    if (peer.second.participantIndex > participantIndex)
    {
      peer.second.participantIndex -= 1;
    }
  }

  participantMutex_.unlock();
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


void StatisticsWindow::receiveDelay(uint32_t sessionID, QString type, int32_t delay)
{
  if(peers_.find(sessionID) != peers_.end())
  {
    if(type == "video" || type == "Video")
    {
      peers_.at(sessionID).videoDelay = delay;
    }
    else if(type == "audio" || type == "Audio")
    {
      peers_.at(sessionID).audioDelay = delay;
    }
  }
}


void StatisticsWindow::presentPackage(uint32_t sessionID, QString type)
{
  Q_ASSERT(peers_.find(sessionID) != peers_.end());
  if(peers_.find(sessionID) != peers_.end())
  {
    if(type == "video" || type == "Video")
    {
      updateFramerateBuffer(peers_.at(sessionID).videoPackets, peers_.at(sessionID).videoIndex, 0);
    }
  }
}


void StatisticsWindow::addEncodedPacket(QString type, uint32_t size)
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
  // delete previous value from ring-buffer
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

  // set timestamp indexes to ringbuffer
  if(index == 0)
  {
    currentTs = BUFFERSIZE - 1;
    previousTs = BUFFERSIZE - 2;
  }
  else if (index == 1)
  {
    currentTs = index - 1; // equals to 0
    previousTs = BUFFERSIZE - 1;
  }
  else
  {
    currentTs = index - 1;
    previousTs = index - 2;
  }

  // sum all bytes and time intervals in ring-buffer for specified timeperiod
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

  // calculate framerate and the average amount of bits per timeinterval (bitrate)
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
    qDebug() << "Settings," << metaObject()->className() << "Couldn't find correct filter for buffer status:" << filter;
  }
  bufferMutex_.unlock();
}


void StatisticsWindow::packetDropped(QString filter)
{
  ++packetsDropped_;
  bufferMutex_.lock();
  if(buffers_.find(filter) != buffers_.end())
  {
    ++buffers_[filter].dropped;
    dirtyBuffers_ = true;
  }
  else
  {
    qDebug() << "Settings," << metaObject()->className() << ": Couldn't find correct filter for dropped packet:" << filter;
  }
  bufferMutex_.unlock();
}


#ifdef QT_CHARTS_LIB
void StatisticsWindow::visualizeDataToSeries(std::deque<float>& data)
{
  ui_->fps_graph->setDragMode(QGraphicsView::NoDrag);
  ui_->fps_graph->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  ui_->fps_graph->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  ui_->fps_graph->setScene(new QGraphicsScene);

  QtCharts::QChart* chart = new QtCharts::QChart;
  QtCharts::QLineSeries* series = new QtCharts::QLineSeries;

  for (int i = data.size() -1; i >= 0; --i)
  {
    series->append(i, data.at(i));
  }

  chart->addSeries(series);

  // x-axis
  QtCharts::QValueAxis* xAxis = new QtCharts::QValueAxis();
  xAxis->setMin(0);
  xAxis->setMax(60);
  xAxis->setReverse(true);
  //xAxis->setTitleText("seconds");
  chart->addAxis(xAxis, Qt::AlignBottom);
  series->attachAxis(xAxis);

  // y-axis, framerate
  QtCharts::QValueAxis* yAxis = new QtCharts::QValueAxis();
  chart->addAxis(yAxis, Qt::AlignRight);
  yAxis->setMin(0);
  yAxis->setMax(framerate_ + 10);
  //yAxis->setTitleText("fps");
  series->attachAxis(yAxis);

  chart->legend()->hide();
  chart->setMinimumSize(480, 200);

  ui_->fps_graph->scene()->addItem(chart);
}
#endif


void StatisticsWindow::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);

  if(lastTabIndex_ != ui_->Statistics_tabs->currentIndex()
     || guiUpdates_*UPDATEFREQUENCY < guiTimer_.elapsed())
  {
    ++guiUpdates_;

    // update only the tab which is visible to reduce processing
    switch(ui_->Statistics_tabs->currentIndex())
    {
    case SIP_TAB:
    {
      // do nothing since this is not continous data
      break;
    }
    case PARTICIPANT_TAB:
    {
      for(auto& d : peers_)
      {
        participantMutex_.lock();
        if (d.second.participantIndex >= ui_->participantTable->rowCount())
        {
          participantMutex_.unlock();
          printProgramError(this, "Faulty pariticipantIndex detected in peer!");
          return;
        }

        ui_->participantTable->setItem(d.second.participantIndex, 3,
                                       new QTableWidgetItem(QString::number(d.second.audioDelay) + " ms"));
        ui_->participantTable->setItem(d.second.participantIndex, 4,
                                       new QTableWidgetItem(QString::number(d.second.videoDelay) + " ms"));

        float framerate = 0;
        uint32_t videoBitrate = bitrate(d.second.videoPackets, d.second.videoIndex, framerate);
        Q_UNUSED(videoBitrate);

        ui_->participantTable->setItem
            (d.second.participantIndex, 5, new QTableWidgetItem( QString::number(framerate, 'g', FPSPRECISION)));
        participantMutex_.unlock();
      }
      break;
    }
    case NETWORK_TAB:
    {
      sendMutex_.lock();
      ui_->packets_sent_value->setText( QString::number(sendPacketCount_));
      ui_->data_sent_value->setText( QString::number(transferredData_));
      sendMutex_.unlock();
      receiveMutex_.lock();
      ui_->packets_received_value->setText( QString::number(receivePacketCount_));
      ui_->data_received_value->setText( QString::number(receivedData_));
      receiveMutex_.unlock();

      ui_->packets_skipped_value->setText(QString::number(packetsDropped_));
      break;
    case MEDIA_TAB:
        float framerate = 0.0f;
        uint32_t videoBitrate = bitrate(videoPackets_, videoIndex_, framerate);
        ui_->video_bitrate_value->setText
            ( QString::number(videoBitrate) + " kbit/s" );

        ui_->encoded_framerate_value->setText
            ( QString::number(framerate, 'g', FPSPRECISION) + " fps" );

#ifdef QT_CHARTS_LIB
        if(lastTabIndex_ != MEDIA_TAB)
        {
          framerates_.clear();
        }
        framerates_.push_front(framerate);

        if (framerates_.size() > GRAPHSIZE)
        {
          framerates_.pop_back();
        }

        visualizeDataToSeries(framerates_);
#endif

        ui_->encode_delay_value->setText( QString::number(videoEncDelay_) + " ms." );
        ui_->audio_delay_value->setText( QString::number(audioEncDelay_) + " ms." );

        float audioFramerate = 0.0f; // not interested in this at the moment.
        uint32_t audioBitrate = bitrate(audioPackets_, audioIndex_, audioFramerate);
        ui_->audio_bitrate_value->setText
            ( QString::number(audioBitrate) + " kbit/s" );
        break;
      }
    case FILTER_TAB:
    {

      if(dirtyBuffers_)
      {
        uint32_t totalBuffers = 0;

        bufferMutex_.lock();
        for(auto& it : buffers_)
        {
          totalBuffers += it.second.bufferStatus;

          if (it.second.tableIndex >= ui_->filterTable->rowCount())
          {
            bufferMutex_.unlock();
            printProgramError(this, "Invalid filtertable index detected!", {"Name"}, {it.first});
            return;
          }

          filterTableMutex_.lock();
          ui_->filterTable->setItem(it.second.tableIndex, 0,new QTableWidgetItem(it.first));
          ui_->filterTable->setItem(it.second.tableIndex, 1,new QTableWidgetItem(it.second.TID));
          ui_->filterTable->setItem(it.second.tableIndex, 2,new QTableWidgetItem(QString::number(it.second.bufferStatus) +
                                                                "/" + QString::number(it.second.bufferSize)));
          ui_->filterTable->setItem(it.second.tableIndex, 3,new QTableWidgetItem(QString::number(it.second.dropped)));
          filterTableMutex_.unlock();
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

  QDialog::paintEvent(event);
}


void StatisticsWindow::addSentSIPMessage(QString type, QString message, QString address)
{
  addSIPMessageToList(ui_->sent_list, type, message, address);
}


void StatisticsWindow::addReceivedSIPMessage(QString type, QString message, QString address)
{
  addSIPMessageToList(ui_->received_list, type, message, address);
}


void StatisticsWindow::addSIPMessageToList(QListWidget* list, QString type, QString message, QString address)
{
  QWidget* widget = new QWidget;
  QGridLayout* layout = new QGridLayout(widget);
  widget->setLayout(layout);

  layout->addWidget(new QLabel(type), 0, 0);
  layout->addWidget(new QLabel(address), 0, 1);

  widget->setToolTip(message);

  QListWidgetItem* item = new QListWidgetItem(list);
  item->setSizeHint(QSize(150, 40));

  list->addItem(item);
  list->setItemWidget(item, widget);
}
