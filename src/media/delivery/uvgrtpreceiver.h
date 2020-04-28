#pragma once

#include <uvgrtp/lib.hh>
#include "media/processing/filter.h"

class UvgRTPReceiver : public Filter
{
public:
  UvgRTPReceiver(QString id, StatisticsInterface *stats, DataType type, QString media, uvg_rtp::media_stream *mstream);
  ~UvgRTPReceiver();

  void receiveHook(uvg_rtp::frame::rtp_frame *frame);

  void uninit();

protected:
  void process();

private:
  // TODO why this must be static

  DataType type_;
  bool addStartCodes_;

  uvg_rtp::media_stream *mstream_;
};
