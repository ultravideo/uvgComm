#pragma once
#include "media/processing/filter.h"
#include <QMutex>
#include <QSemaphore>
#include <uvgrtp/lib.hh>

class StatisticsInterface;

class UvgRTPSender : public Filter
{
public:
  UvgRTPSender(QString id, StatisticsInterface *stats, DataType type,
               QString media, uvg_rtp::media_stream *mstream);
  ~UvgRTPSender();

  void updateSettings();

protected:
  void process();

private:
  DataType type_;
  bool removeStartCodes_;

  uvg_rtp::media_stream *mstream_;
  uint64_t frame_;
  rtp_format_t dataFormat_;
  int rtpFlags_;
};
