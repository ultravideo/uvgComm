#pragma once

#include <uvgrtp/lib.hh>

#include "media/processing/filter.h"

#include <QFuture>

class UvgRTPReceiver : public Filter
{
  Q_OBJECT
public:
  UvgRTPReceiver(uint32_t sessionID,
                 QString id,
                 StatisticsInterface *stats,
                 std::shared_ptr<ResourceAllocator> hwResources,
                 DataType type,
                 QString media,
                 uint32_t localSSRC,
                 uint32_t remoteSSRC,
                 uvgrtp::media_stream *stream,
                 bool runZRTP);
  ~UvgRTPReceiver();

  void receiveHook(uvg_rtp::frame::rtp_frame *frame);

protected:
  void process();

signals:
  void zrtpFailure(uint32_t sessionID);

private:

  void uninit();


  void processRTCPSenderReport(std::unique_ptr<uvgrtp::frame::rtcp_sender_report> sr);

  bool discardUntilIntra_;

  uint16_t lastSeq_;
  uint32_t sessionID_;

  uint32_t localSSRC_;
  uint32_t remoteSSRC_;
  uvgrtp::media_stream* stream_;

  int64_t lastSEITime_;

  QFuture<rtp_error_t> futureRes_;
};
