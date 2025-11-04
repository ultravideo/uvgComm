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
               QString media, uint32_t localSSRC, uint32_t remoteSSRC,
               uvgrtp::media_stream* stream, bool runZRTP);
  ~UvgRTPSender();

  void updateSettings();

  void startForwarding(uint32_t remoteSSRC, int afterFrames);
  void stopForwarding(uint32_t remoteSSRC, int afterFrames);

  void rtt(uint32_t localSSRC, uint32_t remoteSSRC, double time);

signals:
  void rttReceived(uint32_t ssrc, double time);
  void zrtpFailure(uint32_t sessionID);

protected:
  void process();

private:

  void uninit();

  void processRTCPReceiverReport(std::unique_ptr<uvgrtp::frame::rtcp_receiver_report> rr);

  void sendAPP(uint32_t remoteSSRC, int afterFrames, const char *name, uint8_t subtype);

  uvgrtp::media_stream* stream_;

  QFutureWatcher<uvg_rtp::media_stream *> watcher_;
  uint32_t sessionID_;
  rtp_format_t dataFormat_;
  int rtpFlags_;

  int32_t framerateNumerator_;
  int32_t framerateDenominator_;

  QFuture<rtp_error_t> futureRes_;

  uint32_t previousTimestamp_;


};
