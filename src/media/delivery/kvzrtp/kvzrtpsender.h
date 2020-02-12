#pragma once
#include "media/processing/filter.h"
#include <QMutex>
#include <QSemaphore>
#include <kvzrtp/lib.hh>

class StatisticsInterface;

class KvzRTPSender : public Filter
{
public:
  KvzRTPSender(QString id, StatisticsInterface *stats, DataType type,
               QString media, kvz_rtp::media_stream *mstream);
  ~KvzRTPSender();

  void updateSettings();

  void start();
  void stop();

protected:
  void process();

private:
  DataType type_;
  bool removeStartCodes_;

  kvz_rtp::media_stream *mstream_;
  uint64_t frame_;
  rtp_format_t dataFormat_;
};
