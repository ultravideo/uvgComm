#pragma once
#include "statisticsinterface.h"

#include <QDialog>
#include <QMutex>

class QStringListModel;

namespace Ui {
class StatisticsWindow;
}

class StatisticsWindow : public QDialog, public StatisticsInterface
{
  Q_OBJECT

public:
  explicit StatisticsWindow(QWidget *parent);
  virtual ~StatisticsWindow();

  void paintEvent(QPaintEvent *event);
  void closeEvent(QCloseEvent *event);

  virtual void addNextInterface(StatisticsInterface* next);
  virtual void videoInfo(double framerate, QSize resolution);
  virtual void audioInfo(uint32_t sampleRate, uint16_t channelCount);
  virtual void addParticipant(QString ip, QString audioPort, QString videoPort);
  virtual void removeParticipant(QString ip);
  virtual void sendDelay(QString type, uint32_t delay);
  virtual void receiveDelay(uint32_t peer, QString type, int32_t delay);
  virtual void addEncodedPacket(QString type, uint16_t size);
  virtual void addSendPacket(uint16_t size);
  virtual void addReceivePacket(uint16_t size);
  virtual void addFilter(QString filter, uint64_t TID);
  virtual void removeFilter(QString filter);
  virtual void updateBufferStatus(QString filter, uint16_t buffersize, uint16_t maxBufferSize);
  virtual void packetDropped(QString filter);

private:

  struct PacketInfo
  {
    int64_t timestamp;
    uint16_t size;
  };

  struct Delays
  {
    int32_t video;
    int32_t audio;
    bool active;
  };

  std::vector<Delays> delays_;

  uint32_t totalBuffers();
  uint32_t bitrate(std::vector<PacketInfo*>& packets, uint32_t index, float &framerate);

  Ui::StatisticsWindow *ui_;

  struct FilterStatus
  {
    uint32_t bufferStatus;
    QString TID;
    uint32_t bufferSize;
    uint32_t dropped;
  };

  std::map<QString, FilterStatus> buffers_;

  // mutexes to prevent simultanious recording of certain statistics
  QMutex initMutex_;
  QMutex receiveMutex_;
  QMutex sendMutex_;
  QMutex bufferMutex_;

  // should the buffervalue be updated in next paintEvent
  bool dirtyBuffers_;

  uint16_t framerate_; // rounded down currently

  uint32_t videoIndex_;
  std::vector<PacketInfo*> videoPackets_;

  uint32_t audioIndex_;
  std::vector<PacketInfo*> audioPackets_;

  uint64_t sendPacketCount_;
  uint64_t transferredData_;

  uint64_t receivePacketCount_;
  uint64_t receivedData_;

  uint64_t packetsDropped_;

  uint32_t lastVideoBitrate_;
  uint32_t lastAudioBitrate_;

  float lastVideoFrameRate_;
  float lastAudioFrameRate_;

  uint16_t audioEncDelay_;
  uint16_t videoEncDelay_;
};
