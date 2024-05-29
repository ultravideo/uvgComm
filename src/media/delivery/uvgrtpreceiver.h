#pragma once

#include <uvgrtp/lib.hh>

#include "media/processing/filter.h"

class UvgRTPReceiver : public Filter
{
  Q_OBJECT
public:
  UvgRTPReceiver(uint32_t sessionID, QString id, StatisticsInterface *stats,
                 std::shared_ptr<ResourceAllocator> hwResources, DataType type,
                 QString media, uvg_rtp::media_stream * mstream,
                 uint32_t localSSRC = 0, uint32_t remoteSSRC = 0);
  ~UvgRTPReceiver();

  void receiveHook(uvg_rtp::frame::rtp_frame *frame);

  void uninit();

protected:
  void process();

signals:
  void zrtpFailure(uint32_t sessionID);

private:

  void processRTCPSenderReport(std::unique_ptr<uvgrtp::frame::rtcp_sender_report> sr);

  bool discardUntilIntra_;

  uint16_t lastSeq_;
  uint32_t sessionID_;

  uvg_rtp::media_stream * mstream_;

  uint32_t localSSRC_;
  uint32_t remoteSSRC_;
};
