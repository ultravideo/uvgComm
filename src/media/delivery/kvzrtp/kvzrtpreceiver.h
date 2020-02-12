#pragma once

#include <kvzrtp/lib.hh>
#include "media/processing/filter.h"

class KvzRTPReceiver : public Filter
{
public:
  KvzRTPReceiver(QString id, StatisticsInterface *stats, DataType type, QString media, kvz_rtp::media_stream *mstream);
  ~KvzRTPReceiver();

  void receiveHook(kvz_rtp::frame::rtp_frame *frame);

  void uninit();
  void start();

protected:
  void process();

private:
  // TODO why this must be static

  DataType type_;
  bool addStartCodes_;

  kvz_rtp::media_stream *mstream_;
};
