#include "statisticswindow.h"

#include "ui_statisticswindow.h"

#include "common.h"

#include <QCloseEvent>
#include <QDateTime>


#ifdef QT_CHARTS_LIB
#include <QtCharts/QValueAxis>
#include <QtCharts/QAbstractAxis>
#endif

const int BUFFERSIZE = 65536;

const int GRAPHSIZE = 60;

const int FPSPRECISION = 4;

const int UPDATEFREQUENCY = 1000;

enum TabType {
  SIP_TAB = 0, PARAMETERS_TAB = 1, DELIVERY_TAB = 2, FILTER_TAB = 3, PERFORMANCE_TAB = 4
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

  fillTableHeaders(ui_->participantTable, participantMutex_,
                          {"IP", "Audio Port", "Video Port"});
  fillTableHeaders(ui_->filterTable, filterTableMutex_,
                          {"Filter", "TID", "Buffer Size", "Dropped"});
  fillTableHeaders(ui_->performance_table, participantMutex_,
                          {"Name", "Audio delay", "Video delay", "Video fps"});

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
  ui_->value_framerate->setText( QString::number(framerate, 'g', FPSPRECISION)+" fps");
  ui_->value_resolution->setText( QString::number(resolution.width()) + "x"
                          + QString::number(resolution.height()));
}


void StatisticsWindow::audioInfo(uint32_t sampleRate, uint16_t channelCount)
{
  //ui_->a_framerate_value->setText(QString::number(framerate)+"fps");
  if(sampleRate == 0 || sampleRate == 4294967295)
  {
    ui_->value_channels->setText("No Audio");
    ui_->value_samplerate->setText("No Audio");
  }
  else
  {
    ui_->value_channels->setText(QString::number(channelCount));
    ui_->value_samplerate->setText(QString::number(sampleRate) + " Hz");
  }
}


void StatisticsWindow::addParticipant(uint32_t sessionID, QString ip,
                                      QString audioPort, QString videoPort)
{
  int rowIndex = addTableRow(ui_->participantTable, participantMutex_,
                                    {ip, audioPort, videoPort});
  int rowIndex2 = addTableRow(ui_->performance_table, participantMutex_,
                                    {ip, "- ms", "- ms", "- fps"});

  Q_ASSERT(rowIndex == rowIndex2);

  peers_[sessionID] = {0, std::vector<PacketInfo*>(BUFFERSIZE, nullptr),
                       0, 0, rowIndex};
}


void StatisticsWindow::addFilter(QString filter, uint64_t TID)
{
  int rowIndex = addTableRow(ui_->filterTable, filterTableMutex_,
                                  {filter, QString::number(TID), "-/-", "0"});

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
    printProgramWarning(this, "Tried to remove non-existing filter.",
                          {"Name"}, {filter});
    return;
  }
  bufferMutex_.unlock();

  filterTableMutex_.lock();
  if (ui_->filterTable->rowCount() <= buffers_[filter].tableIndex)
  {
    printProgramWarning(this, "Filter doesn't exist in filter table when removing.",
                          {"Name"}, {filter});
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

  int participantIndex = peers_[sessionID].tableIndex;

  // check that index points to a valid row
  if (ui_->participantTable->rowCount() <= participantIndex ||
      ui_->performance_table->rowCount() <= participantIndex)
  {
    participantMutex_.unlock();
    printProgramWarning(this, "Missing participant row for participant");
    return;
  }

  // remove row from UI
  ui_->participantTable->removeRow(participantIndex);
  ui_->performance_table->removeRow(participantIndex);

  // adjust the rest of the peers if needed
  for (auto &peer : peers_)
  {
    if (peer.second.tableIndex > participantIndex)
    {
      peer.second.tableIndex -= 1;
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
      updateFramerateBuffer(peers_.at(sessionID).videoPackets,
                            peers_.at(sessionID).videoIndex, 0);
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


void StatisticsWindow::updateFramerateBuffer(std::vector<PacketInfo*>& packets,
                                             uint32_t& index, uint32_t size)
{
  // delete previous value from ring-buffer
  if(packets[index%BUFFERSIZE])
  {
    delete packets.at(index%BUFFERSIZE);
  }

  packets[index%BUFFERSIZE] = new PacketInfo{QDateTime::currentMSecsSinceEpoch(), size};
  ++index;
}


uint32_t StatisticsWindow::bitrate(std::vector<PacketInfo*>& packets,
                                   uint32_t index, float& framerate)
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
    timeInterval += packets[currentTs%BUFFERSIZE]->timestamp
                      - packets[previousTs%BUFFERSIZE]->timestamp;

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


void StatisticsWindow::updateBufferStatus(QString filter, uint16_t buffersize,
                                          uint16_t maxBufferSize)
{
  bufferMutex_.lock();
  if(buffers_.find(filter) != buffers_.end())
  {
    if(buffers_[filter].bufferStatus != buffersize ||
       buffers_[filter].bufferSize != maxBufferSize)
    {
      dirtyBuffers_ = true;
      buffers_[filter].bufferStatus = buffersize;
      buffers_[filter].bufferSize = maxBufferSize;
    }
  }
  else
  {
    printProgramWarning(this, "Couldn't find correct filter for buffer status",
                        "Filter", filter);
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
    printProgramWarning(this, "Couldn't find correct filter for dropped packet",
                        "Filter", filter);
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
      // do nothing since SIP has no continous data
      break;
    }
    case PARAMETERS_TAB:
    {
      // do nothing since parameters have no continous data
      break;
    }
    case DELIVERY_TAB:
    {
      sendMutex_.lock();
      ui_->packets_sent_value->setText( QString::number(sendPacketCount_));
      ui_->data_sent_value->setText( QString::number(transferredData_));
      sendMutex_.unlock();
      receiveMutex_.lock();
      ui_->packets_received_value->setText( QString::number(receivePacketCount_));
      ui_->data_received_value->setText( QString::number(receivedData_));
      receiveMutex_.unlock();

      break;
    case PERFORMANCE_TAB:
        float framerate = 0.0f;
        uint32_t videoBitrate = bitrate(videoPackets_, videoIndex_, framerate);
        ui_->video_bitrate_value->setText
            ( QString::number(videoBitrate) + " kbit/s" );

        ui_->encoded_framerate_value->setText
            ( QString::number(framerate, 'g', FPSPRECISION) + " fps" );

#ifdef QT_CHARTS_LIB
        if(lastTabIndex_ != PERFORMANCE_TAB)
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

        // fill table
        for(auto& d : peers_)
        {
          participantMutex_.lock();
          if (d.second.tableIndex >= ui_->performance_table->rowCount())
          {
            participantMutex_.unlock();
            printProgramError(this, "Faulty pariticipantIndex detected in peer!");
            return;
          }

          int audioDelay = d.second.audioDelay;
          QString audioUnit = "ms";
          delayMsConversion(audioDelay, audioUnit);

          int videoDelay = d.second.videoDelay;
          QString videoUnit = "ms";
          delayMsConversion(videoDelay, videoUnit);

          ui_->performance_table->setItem(d.second.tableIndex, 1,
                            new QTableWidgetItem(QString::number(audioDelay) + " " + audioUnit));
          ui_->performance_table->setItem(d.second.tableIndex, 2,
                            new QTableWidgetItem(QString::number(videoDelay) + " " + videoUnit));

          float framerate = 0;
          uint32_t videoBitrate = bitrate(d.second.videoPackets, d.second.videoIndex, framerate);
          Q_UNUSED(videoBitrate);

          ui_->performance_table->setItem(d.second.tableIndex, 3,
                             new QTableWidgetItem( QString::number(framerate, 'g', FPSPRECISION)));

          ui_->performance_table->item(d.second.tableIndex, 1)->setTextAlignment(Qt::AlignHCenter);
          ui_->performance_table->item(d.second.tableIndex, 2)->setTextAlignment(Qt::AlignHCenter);
          ui_->performance_table->item(d.second.tableIndex, 3)->setTextAlignment(Qt::AlignHCenter);
          participantMutex_.unlock();
        }

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
          ui_->filterTable->setItem(it.second.tableIndex, 2,
                                    new QTableWidgetItem(QString::number(it.second.bufferStatus) +
                                                         "/" + QString::number(it.second.bufferSize)));
          ui_->filterTable->setItem(it.second.tableIndex, 3,
                                    new QTableWidgetItem(QString::number(it.second.dropped)));

          ui_->filterTable->item(it.second.tableIndex, 2)->setTextAlignment(Qt::AlignHCenter);
          ui_->filterTable->item(it.second.tableIndex, 3)->setTextAlignment(Qt::AlignHCenter);
          filterTableMutex_.unlock();
        }

        ui_->value_buffers->setText(QString::number(totalBuffers));
        ui_->value_dropped->setText(QString::number(packetsDropped_));
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


void StatisticsWindow::addSentSIPMessage(QString type, QString message,
                                         QString address)
{
  addSIPMessageToList(ui_->sent_list, type, message, address);
}


void StatisticsWindow::addReceivedSIPMessage(QString type, QString message,
                                             QString address)
{
  addSIPMessageToList(ui_->received_list, type, message, address);
}


void StatisticsWindow::addSIPMessageToList(QListWidget* list, QString type,
                                           QString message, QString address)
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


void StatisticsWindow::delayMsConversion(int& delay, QString& unit)
{
  if (delay > 1000)
  {
    delay = (delay + 500)/1000;
    unit = "s";
  }
  else
  {
    unit = "ms";
  }
}


void StatisticsWindow::fillTableHeaders(QTableWidget* table, QMutex& mutex,
                                        QStringList headers)
{
  if (!table)
  {
    return;
  }

  mutex.lock();

  table->setColumnCount(headers.size());
  for (int i = 0; i < headers.size(); ++i)
  {
    table->setHorizontalHeaderItem(i, new QTableWidgetItem(headers.at(i)));
  }

  table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  table->horizontalHeader()->setMinimumHeight(40);

  mutex.unlock();
}


int StatisticsWindow::addTableRow(QTableWidget* table, QMutex& mutex,
                                  QStringList fields)
{
  mutex.lock();
  table->insertRow(table->rowCount());

  for (int i = 0; i < fields.size(); ++i)
  {
    table->setItem(table->rowCount() -1, i, new QTableWidgetItem(fields.at(i)));
    table->item(table->rowCount() -1, i)->setTextAlignment(Qt::AlignHCenter);
  }

  int index = table->rowCount() - 1;
  mutex.unlock();
  return index;
}
