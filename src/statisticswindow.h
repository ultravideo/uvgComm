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
  virtual void audioInfo(uint32_t sampleRate);
  virtual void addParticipant(QString ip, QString port);
  virtual void delayTime(QString type, uint32_t delay);
  virtual void addEncodedVideo(uint16_t size);
  virtual void addSendPacket(uint16_t size);
  virtual void addReceivePacket(uint16_t size);
  virtual void updateBufferStatus(QString filter, uint16_t buffersize);
  virtual void packetDropped();

private:

  uint32_t totalBuffers();

  Ui::StatisticsWindow *ui_;

  std::map<QString, uint16_t> buffers_;

  QMutex receiveMutex_;
  QMutex sendMutex_;
  QMutex bufferMutex_;

  bool dirtyBuffers_;
  // framerate rounded down
  uint16_t framerate_;
  uint16_t bitrateCounter_;
  uint32_t bitrate_;

  uint64_t sendPacketCount_;
  uint64_t transferredData_;

  uint64_t receivePacketCount_;
  uint64_t receivedData_;

  uint64_t packetsDropped_;
};
