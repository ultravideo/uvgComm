#pragma once
#include "media/processing/filter.h"
#include <FramedSource.hh>
#include <QMutex>
#include <QSemaphore>

#include "../rtplib/src/writer.hh"

class StatisticsInterface;

class FramedSourceFilter : public Filter
{
public:
  FramedSourceFilter(QString id, StatisticsInterface *stats, DataType type, QString media, kvz_rtp::writer *writer);
  ~FramedSourceFilter();

  void updateSettings();

  void start();
  void stop();

protected:
  void process();

private:
  DataType type_;
  bool stop_;
  bool removeStartCodes_;

  kvz_rtp::writer *writer_;
  uint64_t frame_;
  rtp_format_t dataFormat_;
};
