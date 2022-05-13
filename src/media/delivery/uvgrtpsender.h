#pragma once
#include "media/processing/filter.h"

#include <uvgrtp/lib.hh>

#include <QMutex>
#include <QSemaphore>
#include <QFutureWatcher>

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

  void processRTCPReceiverReport(std::unique_ptr<uvgrtp::frame::rtcp_receiver_report> rr);

  uvg_rtp::media_stream * mstream_;
  QFutureWatcher<uvg_rtp::media_stream *> watcher_;
  uint32_t sessionID_;
  int rtpFlags_;
};
