#include "statisticswindow.h"

#include "ui_statisticswindow.h"

#include "common.h"
#include "logger.h"

#include <QCloseEvent>
#include <QDateTime>
#include <QFileDialog>
#include <QTextStream>


const int BUFFERSIZE = 65536;

const int FPSPRECISION = 4;

const int CHARTVALUES = 20;
const int RTCP_CHARTVALUES = 12;

enum TabType {
  SIP_TAB = 0, PARAMETERS_TAB = 1, DELIVERY_TAB = 2,
  FILTER_TAB = 3, PERFORMANCE_TAB = 4
};


StatisticsWindow::StatisticsWindow(QWidget *parent) :
QDialog(parent),
StatisticsInterface(),
  sessions_(),
  buffers_(),
  nextFilterID_(1),
  ui_(new Ui::StatisticsWindow),
  sessionMutex_(),
  filterMutex_(),
  sipMutex_(),
  deliveryMutex_(),
  dirtyBuffers_(false),
  videoIndex_(0), // ringbuffer index
  videoPackets_(BUFFERSIZE,nullptr), // ringbuffer
  audioIndex_(0), // ringbuffer index
  audioPackets_(BUFFERSIZE,nullptr), // ringbuffer
  inIndex_(0),
  inBandWidth_(BUFFERSIZE,nullptr),
  outIndex_(0),
  outBandwidth_(BUFFERSIZE,nullptr),
  sendPacketCount_(0),
  transferredData_(0),
  receivePacketCount_(0),
  receivedData_(0),
  packetsDropped_(0),
  videoEncDelayIndex_(0),
  videoEncDelay_(BUFFERSIZE,nullptr),
  audioEncDelayIndex_(0),
  audioEncDelay_(BUFFERSIZE,nullptr),
  guiTimer_(),
  guiUpdates_(0),
  rtcpUpdates_(0),
  lastTabIndex_(254) // an invalid value so we will update the tab immediately
{
  ui_->setupUi(this);

  connect(ui_->update_period, &QAbstractSlider::valueChanged,
          this, &StatisticsWindow::changeUpdatePeriod);

  connect(ui_->sample_window, &QAbstractSlider::valueChanged,
          this, &StatisticsWindow::changeSampleWindow);

  // Initiate all charts

  // Delivery-tab
  ui_->bandwidth_chart->init(800, 8, true, CHARTVALUES, "Bandwidth (kbit/s)");
  ui_->bandwidth_chart->addLine("In");
  ui_->bandwidth_chart->addLine("Out");

  ui_->v_jitter->init( 50, 5, true, RTCP_CHARTVALUES, "Jitter");
  ui_->v_lost->init(   50, 5, true, RTCP_CHARTVALUES, "Lost");
  ui_->a_jitter->init( 50, 5, true, RTCP_CHARTVALUES, "Jitter");
  ui_->a_lost->init(   50, 5, true, RTCP_CHARTVALUES, "Lost");

  // performance-tab
  ui_->v_bitrate_chart->init( 500, 5, true,  CHARTVALUES, "Bit rates (kbit/s)");
  ui_->a_bitrate_chart->init(  50, 5, false, CHARTVALUES, "Bit rates (kbit/s)");
  ui_->v_delay_chart->init(   100, 5, true,  CHARTVALUES, "Latencies (ms)");
  ui_->a_delay_chart->init(    10, 5, false, CHARTVALUES, "Latencies (ms)");
  ui_->v_framerate_chart->init(30, 5, false, CHARTVALUES, "Frame rates (fps)");

  chartVideoID_ = ui_->v_bitrate_chart->addLine("Outgoing");
  chartAudioID_ = ui_->a_bitrate_chart->addLine("Outgoing");

  ui_->v_delay_chart->addLine("Outgoing");
  ui_->a_delay_chart->addLine("Outgoing");

  ui_->v_framerate_chart->addLine("Outgoing");

  // init headers of call parameter table
  fillTableHeaders(ui_->table_outgoing, sessionMutex_,
                          {"Target IP", "Audio Ports", "Video Ports"});
  fillTableHeaders(ui_->table_incoming, sessionMutex_,
                          {"Local Interface IP", "Audio Ports", "Video Ports"});
  fillTableHeaders(ui_->filterTable, filterMutex_,
                          {"Filter", "Info", "TID", "Buffer Size", "Dropped"});
  fillTableHeaders(ui_->sent_list, sipMutex_,
                          {"Header", "Body"});
  fillTableHeaders(ui_->received_list, sipMutex_,
                          {"Header", "Body"});
}


StatisticsWindow::~StatisticsWindow()
{
  delete ui_;
}


void StatisticsWindow::showEvent(QShowEvent * event)
{
  Q_UNUSED(event)
  // start refresh timer
  clearCharts();

  // this makes sure the window does not open outside the screen above the parent.
  this->setGeometry(QStyle::alignedRect(Qt::LeftToRight,
                                        Qt::AlignHCenter,
                                        this->size(),
                                        parentWidget()->geometry()));

}


void StatisticsWindow::closeEvent(QCloseEvent *event)
{
  Q_UNUSED(event)
  accept();
}


void StatisticsWindow::videoInfo(double framerate, QSize resolution)
{
  // done only once, so setting ui directly is ok.
  ui_->value_framerate->setText( QString::number(framerate, 'g', FPSPRECISION)+" fps");
  ui_->value_resolution->setText( QString::number(resolution.width()) + "x"
                          + QString::number(resolution.height()));

  // set max framerate as this. Set the y-line every 5 fps, 10 fps if fps is over 60
  if (framerate <= 60)
  {
    ui_->v_framerate_chart->init(framerate, framerate/5, false,
                                 CHARTVALUES, "Frame rates (fps)");
  }
  else
  {
    ui_->v_framerate_chart->init(framerate, framerate/10, false,
                                 CHARTVALUES, "Frame rates (fps)");
  }
}


void StatisticsWindow::audioInfo(uint32_t sampleRate, uint16_t channelCount)
{
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

void StatisticsWindow::addSession(uint32_t sessionID)
{
  if (sessions_.find(sessionID) != sessions_.end())
  {
    Logger::getLogger()->printProgramError(this, "Session already exists");
    return;
  }

  sessions_[sessionID] = {0, std::vector<ValueInfo*>(BUFFERSIZE, nullptr),
                          0, std::vector<ValueInfo*>(BUFFERSIZE, nullptr),
                          0, std::vector<ValueInfo*>(BUFFERSIZE, nullptr),
                          0, std::vector<ValueInfo*>(BUFFERSIZE, nullptr),
                          0, std::vector<ValueInfo*>(BUFFERSIZE, nullptr),
                          0, std::vector<ValueInfo*>(BUFFERSIZE, nullptr),
                          0,0, 0,0, // jitter and lost
                          -1};
}


void StatisticsWindow::incomingMedia(uint32_t sessionID, QString name, QStringList& ipList,
                                     QStringList &audioPorts, QStringList &videoPorts)
{
  if (ipList.size() == 0)
  {
    return;
  }

  addMedia(ui_->table_incoming, sessionID, ipList, audioPorts, videoPorts);

  ui_->v_delay_chart->addLine(name);
  ui_->a_delay_chart->addLine(name);
  ui_->v_bitrate_chart->addLine(name);
  ui_->a_bitrate_chart->addLine(name);
  ui_->v_framerate_chart->addLine(name);
}


void StatisticsWindow::outgoingMedia(uint32_t sessionID, QString name, QStringList& ipList,
                                     QStringList& audioPorts, QStringList& videoPorts)
{
  addMedia(ui_->table_outgoing, sessionID, ipList, audioPorts, videoPorts);

  ui_->v_jitter->addLine(name);
  ui_->a_jitter->addLine(name);
  ui_->v_lost->addLine(name);
  ui_->a_lost->addLine(name);
}


void StatisticsWindow::addMedia(QTableWidget* table, uint32_t sessionID, QStringList& ipList,
                                QStringList audioPorts, QStringList videoPorts)
{
  if (sessions_.find(sessionID) == sessions_.end())
  {
    Logger::getLogger()->printProgramError(this, "Session for media doesn't exist");
    return;
  }

  int index = addTableRow(table, sessionMutex_,
                          {combineList(ipList), combineList(audioPorts),
                           combineList(videoPorts)});

  if (sessions_[sessionID].tableIndex == -1 || sessions_[sessionID].tableIndex == index)
  {
    sessions_[sessionID].tableIndex = index;
  }
  else
  {
    Logger::getLogger()->printProgramError(this, "Wrong table index detected in sessions for media!");
    return;
  }
}

QString StatisticsWindow::combineList(QStringList &list)
{
  QString listed = "";

  for (int i = list.size() - 1; i >= 0; --i)
  {
    // don't record anything if we have to of the same strings
    if (i > 0 && list[i] != list[i - 1])
    {
      listed += list[i];
      listed += ", ";
    }
    else if (i == 0)
    {
      listed += list[i];
    }
  }

  return listed;
}

uint32_t StatisticsWindow::addFilter(QString type, QString identifier, uint64_t TID)
{
  QString threadID = QString::number(TID);
  threadID = threadID.rightJustified(5, '0');

  int rowIndex = addTableRow(ui_->filterTable, filterMutex_,
                                  {type, identifier, threadID, "-/-", "0"});

  filterMutex_.lock();
  uint32_t id = nextFilterID_;
  ++nextFilterID_;
  if (nextFilterID_ >= UINT32_MAX - 2)
  {
    nextFilterID_ = 10;
  }
  buffers_[id] = FilterStatus{0,QString::number(TID), 0, 0, rowIndex};
  filterMutex_.unlock();

  return id;
}


void StatisticsWindow::removeFilter(uint32_t id)
{
  filterMutex_.lock();
  if (buffers_.find(id) == buffers_.end())
  {
    filterMutex_.unlock();
    Logger::getLogger()->printProgramWarning(this, "Tried to remove non-existing filter.",
                          {"Id"}, {QString::number(id)});
    return;
  }
  if (ui_->filterTable->rowCount() < buffers_[id].tableIndex)
  {
    filterMutex_.unlock();
    Logger::getLogger()->printProgramWarning(this, "Filter doesn't exist in filter table when removing.",
                                             {"Id: Table size vs expected place"}, 
                                             {QString::number(id) + ":" + 
                                              QString::number(ui_->filterTable->rowCount())
                                              + " vs " + QString::number(buffers_[id].tableIndex)});
    return;
  }

  ui_->filterTable->removeRow(buffers_[id].tableIndex);

  // adjust all existing indexes
  for (auto& buffer: buffers_)
  {
    if (buffers_[id].tableIndex < buffer.second.tableIndex)
    {
      buffer.second.tableIndex -= 1;
    }
  }

  buffers_.erase(id);
  filterMutex_.unlock();
}


void StatisticsWindow::removeSession(uint32_t sessionID)
{
  // check that peer exists
  if (sessions_.find(sessionID) == sessions_.end())
  {
    return;
  }

  sessionMutex_.lock();

  int index = sessions_[sessionID].tableIndex;

  // check that index points to a valid row
  if (ui_->table_incoming->rowCount() <= index ||
      ui_->table_outgoing->rowCount() <= index)
  {
    sessionMutex_.unlock();
    Logger::getLogger()->printProgramWarning(this, "Missing participant row for participant");
    return;
  }

  // remove row from UI
  ui_->table_incoming->removeRow(index);
  ui_->table_outgoing->removeRow(index);

  // adjust the rest of the peers if needed
  for (auto &peer : sessions_)
  {
    if (peer.second.tableIndex > index)
    {
      --peer.second.tableIndex;
    }
  }

  index += 2; // +1 because it is an ID, not index and +1 for local before peers.

  // remove line from all charts. Charts automatically adjust their lineID:s
  // after removal
  ui_->v_bitrate_chart->removeLine(index);
  ui_->a_bitrate_chart->removeLine(index);
  ui_->v_delay_chart->removeLine(index);
  ui_->a_delay_chart->removeLine(index);
  ui_->v_framerate_chart->removeLine(index);

  // these do not have local so -1 is needed
  ui_->a_jitter->removeLine(index - 1);
  ui_->v_jitter->removeLine(index - 1);
  ui_->a_lost->removeLine(index - 1);
  ui_->v_lost->removeLine(index - 1);

  // TODO: There is still unreleased memory in session!!

  sessions_.erase(sessionID);


  sessionMutex_.unlock();
}


void StatisticsWindow::sendDelay(QString type, uint32_t delay)
{
  if(type == "video" || type == "Video")
  {
    updateValueBuffer(videoEncDelay_,
                      videoEncDelayIndex_, delay);
  }
  else if(type == "audio" || type == "Audio")
  {
    updateValueBuffer(audioEncDelay_,
                      audioEncDelayIndex_, delay);
  }
}


void StatisticsWindow::receiveDelay(uint32_t sessionID, QString type, int32_t delay)
{
  if(sessions_.find(sessionID) != sessions_.end())
  {
    if(type == "video" || type == "Video")
    {
      updateValueBuffer(sessions_.at(sessionID).videoDelay,
                        sessions_.at(sessionID).videoDelayIndex, delay);
    }
    else if(type == "audio" || type == "Audio")
    {
      updateValueBuffer(sessions_.at(sessionID).audioDelay,
                        sessions_.at(sessionID).audioDelayIndex, delay);
    }
  }
}


void StatisticsWindow::presentPackage(uint32_t sessionID, QString type)
{
  Q_ASSERT(sessions_.find(sessionID) != sessions_.end());
  if(sessions_.find(sessionID) != sessions_.end())
  {
    if(type == "video" || type == "Video")
    {
      updateValueBuffer(sessions_.at(sessionID).pVideoPackets,
                            sessions_.at(sessionID).pVideoIndex, 0);
    }
    else if (type == "audio" || type == "Audio")
    {
      updateValueBuffer(sessions_.at(sessionID).pAudioPackets,
                            sessions_.at(sessionID).pAudioIndex, 0);
    }
  }
}


void StatisticsWindow::addEncodedPacket(QString type, uint32_t size)
{
  if(type == "video" || type == "Video")
  {
    updateValueBuffer(videoPackets_, videoIndex_, size);
  }
  else if(type == "audio" || type == "Audio")
  {
    updateValueBuffer(audioPackets_, audioIndex_, size);
  }
}


void StatisticsWindow::updateValueBuffer(std::vector<ValueInfo*>& packets,
                                             uint32_t& index, uint32_t value)
{
  // delete previous value from ring-buffer
  if(packets[index%BUFFERSIZE])
  {
    delete packets.at(index%BUFFERSIZE);
  }

  // add packet at this timestamp
  packets[index%BUFFERSIZE] = new ValueInfo{QDateTime::currentMSecsSinceEpoch(), value};
  ++index;
}



uint32_t StatisticsWindow::calculateAverageAndRate(std::vector<ValueInfo*>& packets, uint32_t index,
                                                   float& rate, int64_t interval, bool calcData)
{
  if(index == 0)
    return 0;

  int64_t now = QDateTime::currentMSecsSinceEpoch();
  int64_t average = 0;
  uint16_t frames = 0;
  uint32_t currentTs = 0;
  rate = 0.0f;

  // set timestamp indexes to ringbuffer
  if(index == 0)
  {
    currentTs = BUFFERSIZE - 1;
  }
  else
  {
    currentTs = index - 1;
  }

  // sum all values and time intervals in ring-buffer for specified timeperiod
  while(packets[currentTs%BUFFERSIZE] && now - packets[currentTs%BUFFERSIZE]->timestamp
        < interval)
  {
    average += packets[currentTs%BUFFERSIZE]->value;
    ++frames;
    if(currentTs != 0)
    {
      --currentTs;
    }
    else
    {
      currentTs = BUFFERSIZE - 1;
    }
  }

  // calculate frame rate and the average amount of bits per timeinterval (bitrate)
  if(frames > 0)
  {
    // rate per second
    rate = (float)frames*1000/interval;

    if (calcData)
    {
      // return the amount of value per second converted to kbits/s
      return 8*average/(interval);
    }
    else
    {
      // return the average size of value
      return average/frames;
    }
  }
  return 0;
}


uint32_t StatisticsWindow::calculateAverage(std::vector<ValueInfo*>& packets, uint32_t index,
                                            int64_t interval, bool kbitConversion)
{
  float rate = 0.0f;
  return calculateAverageAndRate(packets, index, rate, interval, kbitConversion);
}


void StatisticsWindow::addSendPacket(uint32_t size)
{
  deliveryMutex_.lock();
  ++sendPacketCount_;
  transferredData_ += size;

  updateValueBuffer(outBandwidth_, outIndex_, size);
  deliveryMutex_.unlock();
}


void StatisticsWindow::addReceivePacket(uint32_t sessionID, QString type,
                                        uint32_t size)
{
  deliveryMutex_.lock();
  ++receivePacketCount_;
  receivedData_ += size;

  updateValueBuffer(inBandWidth_, inIndex_, size);
  deliveryMutex_.unlock();

  if(sessions_.find(sessionID) != sessions_.end())
  {
    if(type == "video" || type == "Video")
    {
      updateValueBuffer(sessions_.at(sessionID).videoPackets,
                            sessions_.at(sessionID).videoIndex, size);
    }
    else if (type == "audio" || type == "Audio")
    {
      updateValueBuffer(sessions_.at(sessionID).audioPackets,
                            sessions_.at(sessionID).audioIndex, size);
    }
    else
    {
      Logger::getLogger()->printWarning(this, "Unknown type with receive packet");
    }
  }
}


void StatisticsWindow::addRTCPPacket(uint32_t sessionID, QString type,
                                     uint8_t  fraction,
                                     int32_t  lost,
                                     uint32_t last_seq,
                                     uint32_t jitter)
{
  Logger::getLogger()->printNormal(this, "Got RTCP packet", "Jitter", QString::number(jitter));

  if(sessions_.find(sessionID) != sessions_.end())
  {
    deliveryMutex_.lock();
    if(type == "video" || type == "Video")
    {
      sessions_[sessionID].videoLost = lost;
      sessions_[sessionID].videoJitter = jitter;
    }
    else if (type == "audio" || type == "Audio")
    {
      sessions_[sessionID].audioLost = lost;
      sessions_[sessionID].audioJitter = jitter;
    }
    else
    {
      Logger::getLogger()->printWarning(this, "Unknown type with RTCP packet");
    }

    deliveryMutex_.unlock();
  }
}


void StatisticsWindow::updateBufferStatus(uint32_t id, uint16_t buffersize,
                                          uint16_t maxBufferSize)
{
  filterMutex_.lock();
  if(buffers_.find(id) != buffers_.end())
  {
    if(buffers_[id].bufferStatus != buffersize ||
       buffers_[id].bufferSize != maxBufferSize)
    {
      dirtyBuffers_ = true;
      buffers_[id].bufferStatus = buffersize;
      buffers_[id].bufferSize = maxBufferSize;
    }
  }
  else
  {
    Logger::getLogger()->printProgramWarning(this, "Couldn't find correct filter for buffer status",
                                             "Filter id", QString::number(id));
  }
  filterMutex_.unlock();
}


void StatisticsWindow::packetDropped(uint32_t id)
{
  ++packetsDropped_;
  filterMutex_.lock();
  if(buffers_.find(id) != buffers_.end())
  {
    ++buffers_[id].dropped;
    dirtyBuffers_ = true;
  }
  else
  {
    Logger::getLogger()->printProgramWarning(this, "Couldn't find correct filter for dropped packet",
                                             "Filter id", QString::number(id));
  }
  filterMutex_.unlock();
}


void StatisticsWindow::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);

  // clear old points from charts since they are obsolete
  if(lastTabIndex_ != ui_->Statistics_tabs->currentIndex())
  {
    if (ui_->Statistics_tabs->currentIndex() == PERFORMANCE_TAB)
    {
      ui_->v_bitrate_chart->clearPoints();
      ui_->a_bitrate_chart->clearPoints();
      ui_->v_delay_chart->clearPoints();
      ui_->a_delay_chart->clearPoints();
      ui_->v_framerate_chart->clearPoints();
    }
    else if (ui_->Statistics_tabs->currentIndex() == DELIVERY_TAB)
    {
      ui_->bandwidth_chart->clearPoints();

      ui_->a_jitter->clearPoints();
      ui_->v_jitter->clearPoints();
      ui_->a_lost->clearPoints();
      ui_->v_lost->clearPoints();
    }
  }

  // should we update the outlook of statistics
  if(lastTabIndex_ != ui_->Statistics_tabs->currentIndex()
     || guiUpdates_*ui_->update_period->value() < guiTimer_.elapsed())
  {
    // do not take this account if this was only a tab switch
    if (lastTabIndex_ == ui_->Statistics_tabs->currentIndex())
    {
      ++guiUpdates_;
    }

    lastTabIndex_ = ui_->Statistics_tabs->currentIndex();

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
      deliveryMutex_.lock();
      ui_->packets_sent_value->setText( QString::number(sendPacketCount_));
      ui_->data_sent_value->setText( QString::number(transferredData_));
      ui_->packets_received_value->setText( QString::number(receivePacketCount_));
      ui_->data_received_value->setText( QString::number(receivedData_));

      // bandwidth chart
      float packetRate = 0.0f; // not interested in this at the moment.
      uint32_t inBandwidth = calculateAverageAndRate(inBandWidth_, inIndex_, packetRate, 5000, true);
      uint32_t outBandwidth = calculateAverageAndRate(outBandwidth_, outIndex_, packetRate, 5000, true);
      deliveryMutex_.unlock();

      ui_->bandwidth_chart->addPoint(1, inBandwidth);
      ui_->bandwidth_chart->addPoint(2, outBandwidth);


      break;
    }
    case PERFORMANCE_TAB:
      {
        // how long a tail should we consider in bitrate calculations
        int64_t interval = ui_->update_period->value() * ui_->sample_window->value();

        // calculate local video bitrate and framerate
        float videoFramerate = 0.0f;
        uint32_t videoBitrate = calculateAverageAndRate(videoPackets_, videoIndex_, videoFramerate, interval, true);

        // calculate local audio bitrate
        uint32_t audioBitrate = calculateAverage(audioPackets_, audioIndex_, interval, true);

        uint32_t videoEncoderDelay = calculateAverage(videoEncDelay_, videoEncDelayIndex_, interval, false);
        uint32_t audioEncoderDelay = calculateAverage(audioEncDelay_, audioEncDelayIndex_, interval, false);

        // add points to chart
        ui_->v_bitrate_chart->addPoint(chartVideoID_, videoBitrate);
        ui_->a_bitrate_chart->addPoint(chartAudioID_, audioBitrate);
        ui_->v_delay_chart->addPoint(chartVideoID_, videoEncoderDelay);
        ui_->a_delay_chart->addPoint(chartAudioID_, audioEncoderDelay);
        ui_->v_framerate_chart->addPoint(chartVideoID_, videoFramerate);

        // add points for all existing sessions
        for(auto& d : sessions_)
        {
          sessionMutex_.lock();

          float receiveVideorate = 0; // not shown at the moment. We show presentation framerate instead
          uint32_t videoBitrate = calculateAverageAndRate(d.second.videoPackets, d.second.videoIndex,
                                                          receiveVideorate, interval, true);

          float presentationVideoFramerate = 0;
          calculateAverageAndRate(d.second.pVideoPackets, d.second.pVideoIndex,
                                  presentationVideoFramerate, interval, true);

          uint32_t audioBitrate = calculateAverage(d.second.audioPackets, d.second.audioIndex,
                                                   interval, true);

          uint32_t videoDelay = calculateAverage(d.second.videoDelay, d.second.videoDelayIndex,
                                                 interval, false);
          uint32_t audioDelay = calculateAverage(d.second.audioDelay, d.second.audioDelayIndex,
                                                 interval, false);

          ui_->v_bitrate_chart->addPoint(d.second.tableIndex + 2, videoBitrate);
          ui_->a_bitrate_chart->addPoint(d.second.tableIndex + 2, audioBitrate);
          ui_->v_delay_chart->addPoint(d.second.tableIndex + 2, videoDelay);
          ui_->a_delay_chart->addPoint(d.second.tableIndex + 2, audioDelay);
          ui_->v_framerate_chart->addPoint(d.second.tableIndex + 2, presentationVideoFramerate);

          sessionMutex_.unlock();
        }


        break;
      }
    case FILTER_TAB:
    {
      if(dirtyBuffers_)
      {
        uint32_t totalBuffers = 0;

        filterMutex_.lock();
        for(auto& it : buffers_)
        {
          totalBuffers += it.second.bufferStatus;

          if (it.second.tableIndex >= ui_->filterTable->rowCount() ||
              it.second.tableIndex == -1)
          {
            filterMutex_.unlock();
            Logger::getLogger()->printProgramError(this, "Invalid filtertable index detected!", 
                                                   {"Name"}, {it.first});
            return;
          }

          ui_->filterTable->setItem(it.second.tableIndex, 3,
                                    new QTableWidgetItem(QString::number(it.second.bufferStatus) +
                                                         "/" + QString::number(it.second.bufferSize)));
          ui_->filterTable->setItem(it.second.tableIndex, 4,
                                    new QTableWidgetItem(QString::number(it.second.dropped)));

          ui_->filterTable->item(it.second.tableIndex, 3)->setTextAlignment(Qt::AlignHCenter);
          ui_->filterTable->item(it.second.tableIndex, 4)->setTextAlignment(Qt::AlignHCenter);
        }
        filterMutex_.unlock();

        ui_->value_buffers->setText(QString::number(totalBuffers));
        ui_->value_dropped->setText(QString::number(packetsDropped_));
        dirtyBuffers_ = false;

      }
      break;
    }
    default:
    {
      break;
    }
    }
  }

  if((lastTabIndex_ != DELIVERY_TAB || rtcpUpdates_*5000 < guiTimer_.elapsed()) &&
     ui_->Statistics_tabs->currentIndex() == DELIVERY_TAB)
  {
    // do not take this account if this was only a tab switch
    if (lastTabIndex_ == DELIVERY_TAB)
    {
      ++rtcpUpdates_;
    }

    deliveryMutex_.lock();
    // jitter and lost charts
    for(auto& d : sessions_)
    {
      ui_->v_jitter->addPoint(d.second.tableIndex + 1, d.second.videoJitter);
      ui_->v_lost->addPoint(  d.second.tableIndex + 1, d.second.videoLost);
      ui_->a_jitter->addPoint(d.second.tableIndex + 1, d.second.audioJitter);
      ui_->a_lost->addPoint(  d.second.tableIndex + 1, d.second.audioLost);
    }
    deliveryMutex_.unlock();
  }

  QDialog::paintEvent(event);
}


void StatisticsWindow::addSentSIPMessage(const QString& headerType, const QString& header,
                                         const QString& bodyType, const QString& body)
{
  addTableRow(ui_->sent_list, sipMutex_, {headerType, bodyType}, {header, body});
}


void StatisticsWindow::addReceivedSIPMessage(const QString& headerType, const QString& header,
                                             const QString& bodyType, const QString& body)
{
  int row = addTableRow(ui_->received_list, sipMutex_, {headerType, bodyType}, {header, body});

  sipMutex_.lock();
  ui_->received_list->itemAt(0, row);
  //first->setBackground(QColor(235,235,235));
  ui_->received_list->itemAt(1, row);
  //second->setBackground(QColor(235,235,235));
  sipMutex_.unlock();
}


void StatisticsWindow::delayMsConversion(int& delay, QString& unit)
{
  if (delay >= 1000)
  {
    delay = (delay + 500)/1000;
    unit = "s";
  }
  else if (delay <= -1000)
  {
    delay = (delay - 500)/1000;
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

  table->horizontalHeader()->setSectionsClickable(true);
  table->setSortingEnabled(true);

  mutex.unlock();
}

int StatisticsWindow::addTableRow(QTableWidget* table, QMutex& mutex, QStringList fields,
                                  QString tooltip)
{
  return addTableRow(table, mutex, fields, QStringList{tooltip});
}

int StatisticsWindow::addTableRow(QTableWidget* table, QMutex& mutex,
                                  QStringList fields, QStringList tooltips)
{
  mutex.lock();
  table->insertRow(table->rowCount());

  for (int i = 0; i < fields.size(); ++i)
  {
    QTableWidgetItem* item = new QTableWidgetItem(fields.at(i));
    item->setTextAlignment(Qt::AlignHCenter);
    if (!tooltips.empty())
    {
      // two modes are supported, single tooltip for all and setting from list at beginning
      if (tooltips.size() == 1)
      {
        item->setToolTip(tooltips.at(0));
      }
      else if (tooltips.size() > i)
      {
        item->setToolTip(tooltips.at(i));
      }
    }
    item->setFlags(item->flags() & ~(Qt::ItemIsEditable | Qt::ItemIsSelectable));
    table->setItem(table->rowCount() -1, i, item);
  }

  int index = table->rowCount() - 1;
  mutex.unlock();
  return index;
}


void StatisticsWindow::changeUpdatePeriod(int value)
{
  // this makes limits the update frequency to only discreet values
  // which I think is more suitable for this.
  int limitedValue = static_cast<int>((value + 50)/100);
  limitedValue *= 100;

  // move slider to discreet value
  ui_->update_period->setValue(limitedValue);

  ui_->update_period_label->setText("Update Period: "
                                    + getTimeConversion(limitedValue));

  changeSampleWindow(ui_->sample_window->value());
  clearCharts();
}


void StatisticsWindow::changeSampleWindow(int value)
{
  int sampleWindow = ui_->update_period->value() * value;
  ui_->sample_window_label->setText("Sample Window Length: "
                                    +  getTimeConversion(sampleWindow));
  clearCharts();
}


void StatisticsWindow::clearCharts()
{
  // clear charts
  ui_->v_delay_chart->clearPoints();
  ui_->a_delay_chart->clearPoints();
  ui_->v_bitrate_chart->clearPoints();
  ui_->a_bitrate_chart->clearPoints();
  ui_->v_framerate_chart->clearPoints();

  ui_->a_jitter->clearPoints();
  ui_->v_jitter->clearPoints();
  ui_->a_lost->clearPoints();
  ui_->v_lost->clearPoints();

  //ui_->bandwidth_chart->clearPoints();

  // reset GUI timer so the new frequency works
  guiUpdates_ = 0;
  guiTimer_.restart();

}


QString StatisticsWindow::getTimeConversion(int valueInMs)
{
  // show as seconds
  if (valueInMs >= 1000)
  {
    return (QString::number(valueInMs/1000) + "." + QString::number(valueInMs%1000/100) + " s");
  }
  // show as milliseconds
  return QString::number(valueInMs) + " ms";
}


void StatisticsWindow::on_save_button_clicked()
{
  Logger::getLogger()->printNormal(this, "Saving SIP messages");

  if (ui_->sent_list->columnCount() == 0 ||
      ui_->received_list->columnCount() == 0)
  {
    Logger::getLogger()->printProgramWarning(this, "The column count was too low to read tooltip");
    return;
  }

  QString lineEnd = "\r\n";

  QString text;
  text += "Sent SIP Messages" + lineEnd + lineEnd;

  sipMutex_.lock();
  for (int i = 0; i < ui_->sent_list->rowCount(); ++i)
  {
    text += ui_->sent_list->item(i, 1)->toolTip() + lineEnd;
  }
  sipMutex_.unlock();

  text += "Received SIP Messages" + lineEnd + lineEnd;

  sipMutex_.lock();
  for (int i = 0; i < ui_->received_list->rowCount(); ++i)
  {
    text += ui_->received_list->item(i, 1)->toolTip() + lineEnd;
  }
  sipMutex_.unlock();

  // tr is for text translations
  saveTextToFile(text, tr("Save SIP log"), tr("Text File (*.txt);;All Files (*)"));
}


void StatisticsWindow::on_clear_button_clicked()
{
  Logger::getLogger()->printNormal(this, "Clearing SIP messages");

  sipMutex_.lock();

  //ui_->sent_list->clearContents();
  ui_->sent_list->setRowCount(0);
  //ui_->received_list->clearContents();
  ui_->received_list->setRowCount(0);
  sipMutex_.unlock();
}


void StatisticsWindow::saveTextToFile(const QString& text, const QString &windowCaption,
                                      const QString &options)
{
  if (text == "")
  {
    Logger::getLogger()->printWarning(this, "Tried to save empty text. Not saving");
    return;
  }

  QString fileName = QFileDialog::getSaveFileName(this, windowCaption, "", options);

  if (!fileName.isEmpty())
  {
    QFile file(fileName);

    if (!file.open(QIODevice::WriteOnly))
    {
      Logger::getLogger()->printWarning(this, "Failed to open file");
      return;
    }
    QTextStream fileStream(&file);
    fileStream << text;
  }
}
