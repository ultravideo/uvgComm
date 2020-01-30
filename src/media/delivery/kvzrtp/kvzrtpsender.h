#pragma once
#include "media/processing/filter.h"
#include <QMutex>
#include <QSemaphore>
#include <kvzrtp/writer.hh>

class StatisticsInterface;

class KvzRTPSender : public Filter
{
public:
  KvzRTPSender(QString id, StatisticsInterface *stats, DataType type,
               QString media, kvz_rtp::writer *writer);
  ~KvzRTPSender();

  void updateSettings();

  void start();
  void stop();

protected:
  void process();

private:
  DataType type_;
  bool removeStartCodes_;

  kvz_rtp::writer *writer_;
  uint64_t frame_;
  rtp_format_t dataFormat_;
};
