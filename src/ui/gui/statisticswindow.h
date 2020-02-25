#pragma once
#include "statisticsinterface.h"

#include <QDialog>
#include <QMutex>
#include <QElapsedTimer>

#ifdef QT_CHARTS_LIB
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>

#include <deque>
#endif


class QStringListModel;
class QListWidget;

namespace Ui {
class StatisticsWindow;
}

class StatisticsWindow : public QDialog, public StatisticsInterface
{
  Q_OBJECT

public:
  explicit StatisticsWindow(QWidget *parent);
  virtual ~StatisticsWindow();

  // renders the UI based on which tab is open.
  void paintEvent(QPaintEvent *event);
  void showEvent(QShowEvent * event);
  void closeEvent(QCloseEvent *event);

  // see statisticsInterface for details
  virtual void addNextInterface(StatisticsInterface* next);
  virtual void videoInfo(double framerate, QSize resolution);
  virtual void audioInfo(uint32_t sampleRate, uint16_t channelCount);
  virtual void addParticipant(uint32_t sessionID, QString ip, QString audioPort, QString videoPort);
  virtual void removeParticipant(uint32_t sessionID);
  virtual void sendDelay(QString type, uint32_t delay);
  virtual void presentPackage(uint32_t sessionID, QString type);
  virtual void receiveDelay(uint32_t sessionID, QString type, int32_t delay);
  virtual void addEncodedPacket(QString type, uint32_t size);
  virtual void addSendPacket(uint16_t size);
  virtual void addReceivePacket(uint16_t size);
  virtual void addFilter(QString filter, uint64_t TID);
  virtual void removeFilter(QString filter);
  virtual void updateBufferStatus(QString filter, uint16_t buffersize, uint16_t maxBufferSize);
  virtual void packetDropped(QString filter);
  virtual void addSentSIPMessage(QString type, QString message, QString address);
  virtual void addReceivedSIPMessage(QString type, QString message, QString address);


private:

  // info about one packet for calculating bitrate
  struct PacketInfo
  {
    int64_t timestamp;
    uint32_t size;
  };

  struct PeerInfo
  {
    uint32_t videoIndex;
    std::vector<PacketInfo*> videoPackets;
    int32_t videoDelay;
    int32_t audioDelay;

    int participantIndex;
  };

  std::map<uint32_t, PeerInfo> peers_;

  uint32_t bitrate(std::vector<PacketInfo*>& packets, uint32_t index, float &framerate);
  void updateFramerateBuffer(std::vector<PacketInfo*>& packets, uint32_t& index, uint32_t size);

  void addSIPMessageToList(QListWidget* list, QString type, QString message, QString address);

  void delayMsConversion(int& delay, QString& unit);

#ifdef QT_CHARTS_LIB
  void visualizeDataToSeries(std::deque<float>& data);
#endif

  Ui::StatisticsWindow *ui_;

  struct FilterStatus
  {
    uint32_t bufferStatus;
    QString TID;
    uint32_t bufferSize;
    uint32_t dropped;

    int tableIndex;
  };

  std::map<QString, FilterStatus> buffers_;

  // mutexes to prevent simultanious recording of certain statistics
  QMutex participantMutex_;
  QMutex filterTableMutex_;

  QMutex receiveMutex_;
  QMutex sendMutex_;
  QMutex bufferMutex_;

  // should the buffervalue be updated in next paintEvent
  bool dirtyBuffers_;

  double framerate_; // rounded down currently


  // ring-buffer and its current index
  uint32_t videoIndex_;
  std::vector<PacketInfo*> videoPackets_;

  // ring-buffer and its current index
  uint32_t audioIndex_;
  std::vector<PacketInfo*> audioPackets_;

  uint64_t sendPacketCount_;
  uint64_t transferredData_;
  uint64_t receivePacketCount_;
  uint64_t receivedData_;

  uint64_t packetsDropped_;

#ifdef QT_CHARTS_LIB
  std::deque<float> framerates_;
#endif

  uint16_t audioEncDelay_;
  uint16_t videoEncDelay_;

  // a timer for reducing number of gui updates and making it more readable
  QElapsedTimer guiTimer_;
  qint64 guiUpdates_;

  // for updating the tab as fast as possible
  int lastTabIndex_;
};
