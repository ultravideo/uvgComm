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
  virtual void incomingMedia(uint32_t sessionID, QString name, QStringList& ipList,
                             QStringList& audioPorts, QStringList& videoPorts);
  virtual void outgoingMedia(uint32_t sessionID, QString name, QStringList& ipList,
                             QStringList& audioPorts, QStringList& videoPorts);
  virtual void sendDelay(QString type, uint32_t delay);
  virtual void receiveDelay(uint32_t sessionID, QString type, int32_t delay);
  virtual void presentPackage(uint32_t sessionID, QString type);
  virtual void addEncodedPacket(QString type, uint32_t size);

  // delivery
  virtual void addSendPacket(uint32_t size);
  virtual void addReceivePacket(uint32_t sessionID, QString type, uint32_t size);

  // filter
  virtual uint32_t addFilter(QString type, QString identifier, uint64_t TID);
  virtual void removeFilter(uint32_t id);
  virtual void updateBufferStatus(uint32_t id, uint16_t buffersize,
                                  uint16_t maxBufferSize);
  virtual void packetDropped(uint32_t id);

  // sip
  virtual void addSentSIPMessage(const QString& headerType, const QString& header,
                                 const QString& bodyType, const QString& body);
  virtual void addReceivedSIPMessage(const QString& headerType, const QString& header,
                                     const QString& bodyType, const QString& body);

private slots:

  // Rounds the update period to nearest second and updates the label with
  // the current value.
  void changeUpdatePeriod(int value);

  // Changes the label of sample window to reflect current value.
  void changeSampleWindow(int value);

  // these are called automatically because of their naming
  void on_save_button_clicked();
  void on_clear_button_clicked();

private:

  void clearCharts();

  // Info about one packet for calculating bitrate.
  struct ValueInfo
  {
    int64_t timestamp;
    uint32_t value;
  };

  uint32_t calculateAverageAndRate(std::vector<ValueInfo*>& packets, uint32_t index,
                                   float &rate, int64_t interval, bool calcData);

  uint32_t calculateAverage(std::vector<ValueInfo*>& packets, uint32_t index,
                            int64_t interval, bool kbitConversion);



  void updateValueBuffer(std::vector<ValueInfo*>& packets,
                         uint32_t& index, uint32_t value);

  void delayMsConversion(int& delay, QString& unit);

  void fillTableHeaders(QTableWidget* table, QMutex& mutex, QStringList headers);

  // returns the index of added row
  int addTableRow(QTableWidget* table, QMutex& mutex, QStringList fields,
                  QStringList tooltips);

  int addTableRow(QTableWidget* table, QMutex& mutex, QStringList fields,
                  QString tooltip = "");

  void addMedia(QTableWidget* table, uint32_t sessionID, QStringList& ipList,
                QStringList audioPorts, QStringList videoPorts);
  QString combineList(QStringList& list);

  QString getTimeConversion(int valueInMs);

  void saveTextToFile(const QString& text, const QString &windowCaption,
                      const QString &options);

  struct SessionInfo
  {
    // TODO: where are all these deleted?

    // receive buffers for calculating stream size
    uint32_t videoIndex;
    std::vector<ValueInfo*> videoPackets;
    uint32_t audioIndex;
    std::vector<ValueInfo*> audioPackets;

    // presentation buffers for frame rate
    uint32_t pVideoIndex;
    std::vector<ValueInfo*> pVideoPackets;
    uint32_t pAudioIndex;
    std::vector<ValueInfo*> pAudioPackets;

    // delay buffers for calculating average delay
    uint32_t videoDelayIndex;
    std::vector<ValueInfo*> videoDelay;
    uint32_t audioDelayIndex;
    std::vector<ValueInfo*> audioDelay;

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
  std::vector<ValueInfo*> videoPackets_;

  // ring-buffer and its current index
  uint32_t audioIndex_;
  std::vector<ValueInfo*> audioPackets_;

  uint32_t inIndex_;
  std::vector<ValueInfo*> inBandWidth_;
  uint32_t outIndex_;
  std::vector<ValueInfo*> outBandwidth_;

  uint64_t sendPacketCount_;
  uint64_t transferredData_;
  uint64_t receivePacketCount_;
  uint64_t receivedData_;

  uint64_t packetsDropped_;

  // TODO: delete these
  uint32_t videoEncDelayIndex_;
  std::vector<ValueInfo*> videoEncDelay_;
  uint32_t audioEncDelayIndex_;
  std::vector<ValueInfo*> audioEncDelay_;


  // a timer for reducing number of gui updates and making it more readable
  QElapsedTimer guiTimer_;
  qint64 guiUpdates_;

  // for updating the tab as fast as possible
  int lastTabIndex_;

  int chartVideoID_;
  int chartAudioID_;
};
