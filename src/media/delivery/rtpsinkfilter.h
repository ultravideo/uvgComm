#pragma once
#include "media/processing/filter.h"
#include "../rtplib/src/reader.hh"

// Receives RTP packets and sends them forward in filter graph.

class RTPSinkFilter : public Filter
{
public:
  RTPSinkFilter(QString id, StatisticsInterface *stats, DataType type, QString media, kvz_rtp::reader *reader);
  ~RTPSinkFilter();

  void receiveHook(kvz_rtp::frame::rtp_frame *frame);

  void uninit();
  void start();

protected:
  void process();

private:
  // TODO why this must be static

  DataType type_;
  bool addStartCodes_;

  kvz_rtp::reader *reader_;
};
