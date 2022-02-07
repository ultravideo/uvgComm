#pragma once

#include <uvgrtp/lib.hh>
#include <QFutureWatcher>
#include "media/processing/filter.h"

class UvgRTPReceiver : public Filter
{
  Q_OBJECT
public:
  UvgRTPReceiver(uint32_t sessionID, QString id, StatisticsInterface *stats,
                 std::shared_ptr<HWResourceManager> hwResources, DataType type,
                 QString media, QFuture<uvg_rtp::media_stream *> mstream);
  ~UvgRTPReceiver();

  void receiveHook(uvg_rtp::frame::rtp_frame *frame);

  void uninit();

protected:
  void process();

signals:
  void zrtpFailure(uint32_t sessionID);

private:

  bool shouldDiscard(uint16_t frameSeq, uint8_t* payload);

  bool gotSeq_;

  bool discardUntilIntra_;

  uint16_t lastSeq_;

  DataType type_;
  uint32_t sessionID_;

  QFutureWatcher<uvg_rtp::media_stream *> watcher_;
};
