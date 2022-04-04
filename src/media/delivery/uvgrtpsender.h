#pragma once
#include "media/processing/filter.h"
#include <QMutex>
#include <QSemaphore>
#include <QFutureWatcher>
#include <uvgrtp/lib.hh>

class StatisticsInterface;

class UvgRTPSender : public Filter
{
  Q_OBJECT
public:
  UvgRTPSender(uint32_t sessionID, QString id, StatisticsInterface *stats,
               std::shared_ptr<ResourceAllocator> hwResources, DataType type,
               QString media, QFuture<uvg_rtp::media_stream *> mstream);
  ~UvgRTPSender();

  void updateSettings();

protected:
  void process();

signals:
  void zrtpFailure(uint32_t sessionID);

private:
  DataType type_;
  bool removeStartCodes_;

  uvg_rtp::media_stream * mstream_;
  QFutureWatcher<uvg_rtp::media_stream *> watcher_;
  uint64_t frame_;
  uint32_t sessionID_;
  rtp_format_t dataFormat_;
  int rtpFlags_;
};
