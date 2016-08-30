#pragma once

#include "statisticsinterface.h"

#include <QDialog>

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

  void closeEvent(QCloseEvent *event);

  virtual void addNextInterface(StatisticsInterface* next);
  virtual void videoInfo(double framerate, QSize resolution);
  virtual void audioInfo(uint32_t sampleRate);
  virtual void addParticipant(QString ip, QString port);
  virtual void delayTime(QString type, struct timeval);
  virtual void addSendPacket(uint16_t size);
  virtual void updateBufferStatus(QString filter, uint16_t buffersize);

private:
  Ui::StatisticsWindow *ui_;
};
