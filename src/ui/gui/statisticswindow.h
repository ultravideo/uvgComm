#pragma once
#include "statisticsinterface.h"

#include <QDialog>
#include <QMutex>
#include <QElapsedTimer>


class QStringListModel;
class QListWidget;
class QTableWidget;


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
  // use these for inputting data. Some of these record fields to be later
  // set to UI and some of these modify UI directly.

  // session
  virtual void addSession(uint32_t sessionID);
  virtual void removeSession(uint32_t sessionID);

  // media
  virtual void videoInfo(double framerate, QSize resolution);
  virtual void audioInfo(uint32_t sampleRate, uint16_t channelCount);
  virtual void incomingMedia(uint32_t sessionID, QStringList& ipList,
                             QStringList& audioPorts, QStringList& videoPorts);
  virtual void outgoingMedia(uint32_t sessionID, QStringList& ipList,
                             QStringList& audioPorts, QStringList& videoPorts);
  virtual void sendDelay(QString type, uint32_t delay);
  virtual void receiveDelay(uint32_t sessionID, QString type, int32_t delay);
  virtual void presentPackage(uint32_t sessionID, QString type);
  virtual void addEncodedPacket(QString type, uint32_t size);

  // delivery
  virtual void addSendPacket(uint16_t size);
  virtual void addReceivePacket(uint16_t size);

  // filter
  virtual uint32_t addFilter(QString type, QString identifier, uint64_t TID);
  virtual void removeFilter(uint32_t id);
  virtual void updateBufferStatus(uint32_t id, uint16_t buffersize,
                                  uint16_t maxBufferSize);
  virtual void packetDropped(uint32_t id);

  // sip
  virtual void addSentSIPMessage(QString type, QString message, QString address);
  virtual void addReceivedSIPMessage(QString type, QString message, QString address);

private slots:
  void clearGUI(int value);

private:

  // info about one packet for calculating bitrate
  struct PacketInfo
  {
    int64_t timestamp;
    uint32_t size;
  };

  uint32_t bitrate(std::vector<PacketInfo*>& packets, uint32_t index,
                   float &framerate, int64_t interval);
  void updateFramerateBuffer(std::vector<PacketInfo*>& packets,
                             uint32_t& index, uint32_t size);

  void delayMsConversion(int& delay, QString& unit);

  void fillTableHeaders(QTableWidget* table, QMutex& mutex, QStringList headers);

  // returns the index of added row
  int addTableRow(QTableWidget* table, QMutex& mutex, QStringList fields, QString tooltip = "");

  void addMedia(QTableWidget* table, uint32_t sessionID, QStringList& ipList,
                QStringList audioPorts, QStringList videoPorts);
  QString combineList(QStringList& list);

  struct SessionInfo
  {
    uint32_t videoIndex;
    std::vector<PacketInfo*> videoPackets;
    int32_t videoDelay;
    int32_t audioDelay;

    // index for all UI tables this peer is part of
    int tableIndex;
  };

  std::map<uint32_t, SessionInfo> sessions_;

  struct FilterStatus
  {
    uint32_t bufferStatus;
    QString TID;
    uint32_t bufferSize;
    uint32_t dropped;

    int tableIndex;
  };

  std::map<uint32_t, FilterStatus> buffers_;
  uint32_t nextFilterID_;

  Ui::StatisticsWindow *ui_;

  // mutexes to prevent simultanious recording of certain statistics
  QMutex sessionMutex_;
  QMutex filterMutex_;
  QMutex sipMutex_;
  QMutex deliveryMutex_;

  // should the buffervalue be updated in next paintEvent
  bool dirtyBuffers_;

  // ring-buffer and its current index
  uint32_t videoIndex_;
  std::vector<PacketInfo*> videoPackets_;

  // ring-buffer and its current index
  uint32_t audioIndex_;
  std::vector<PacketInfo*> audioPackets_;

  uint32_t inIndex_;
  std::vector<PacketInfo*> inBandWidth_;
  uint32_t outIndex_;
  std::vector<PacketInfo*> outBandwidth_;

  uint64_t sendPacketCount_;
  uint64_t transferredData_;
  uint64_t receivePacketCount_;
  uint64_t receivedData_;

  uint64_t packetsDropped_;

  uint16_t audioEncDelay_;
  uint16_t videoEncDelay_;

  // a timer for reducing number of gui updates and making it more readable
  QElapsedTimer guiTimer_;
  qint64 guiUpdates_;

  // for updating the tab as fast as possible
  int lastTabIndex_;

  int chartVideoID_;
  int chartAudioID_;
};
