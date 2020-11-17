#pragma once

#include <uvgrtp/lib.hh>
#include <QFutureWatcher>
#include "media/processing/filter.h"

class UvgRTPReceiver : public Filter
{
  Q_OBJECT
public:
  UvgRTPReceiver(uint32_t sessionID, QString id, StatisticsInterface *stats, DataType type, QString media,
      QFuture<uvg_rtp::media_stream *> mstream);
  ~UvgRTPReceiver();

  void receiveHook(uvg_rtp::frame::rtp_frame *frame);

  void uninit();

protected:
  void process();

signals:
  void zrtpFailure(uint32_t sessionID);

private:
  // TODO why this must be static

  DataType type_;
  uint32_t sessionID_;
  bool addStartCodes_;

  QFutureWatcher<uvg_rtp::media_stream *> watcher_;
};
