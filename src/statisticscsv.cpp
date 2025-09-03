#include "statisticscsv.h"

#include "logger.h"

#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QDateTime>
#include <QDebug>

StatisticsCSV::StatisticsCSV()
{
  // Create output directory
  QDir().mkpath("stats_output");
}

void StatisticsCSV::addSession(uint32_t sessionID)
{
  // Nothing to do here
}

void StatisticsCSV::removeSession(uint32_t sessionID)
{
  // Write CSV files for all participants in this session
  if (sessionNames_.find(sessionID) == sessionNames_.end())
  {
    Logger::getLogger()->printWarning("CSV Stats", QString("No participants found for session %1").arg(sessionID));
    return;
  }

  Logger::getLogger()->printNormal("CSV Stats", QString("Writing statistics for session %1").arg(sessionID));

  QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
  QString baseFolder = "stats_output";
  QString sessionFolder = baseFolder + "/" + timestamp;

  QDir dir;
  if (!dir.exists(sessionFolder))
  {
    dir.mkpath(sessionFolder);
  }

  QStringList names = sessionNames_.at(sessionID);
  for (const QString& cname : names)
  {
    QString safeName = cname;
    safeName.replace(":", "_"); // make it file-safe
    QString filename = sessionFolder + QString("/participant_%1.csv").arg(safeName);

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
      qWarning() << "Failed to write stats file for" << cname;
      continue;
    }

    QTextStream out(&file);
    out << "Type;Size(Bytes);DecodeTime(ms);Latency(ms);Resolution\n";

    const auto& info = sessionInfo_[cname];

    for (const auto& frame : info.decodedVideoFrames)
    {
      out << "Video,"
          << frame.size << ","
          << frame.decodingTime << ",,"
          << QString("%1x%2").arg(frame.resolution.width()).arg(frame.resolution.height()) << "\n";
    }

    for (const auto& frame : info.decodedAudioFrames)
    {
      out << "Audio,"
          << frame.size << ","
          << frame.decodingTime << ",,\n";
    }

    for (int64_t delay : info.videoLatencies)
    {
      out << "VideoLatency;;;;"
          << delay << "\n";
    }

    for (int64_t delay : info.audioLatencies)
    {
      out << "AudioLatency;;;;"
          << delay << "\n";
    }
  }

  // Save local info (only once per session)
  QString filename = sessionFolder + QString("/local_encoded_video.csv");
  QFile file(filename);
  if (file.open(QIODevice::WriteOnly | QIODevice::Text))
  {
    QTextStream out(&file);
    out << "Frame;Size(Bytes);EncodeTime(ms);PSNR_Y;PSNR_U;PSNR_V;Resolution;Width;Height;Pixels\n";
    unsigned int frame_number = 1;
    for (const auto& frame : localInfo_.encodedVideoFrames)
    {
      out << frame_number++ << ";"
          << frame.size << ";"
          << frame.encodingTime << ";"
          << frame.psnrY << ";"
          << frame.psnrU << ";"
          << frame.psnrV << ";"
          << QString("%1x%2").arg(frame.resolution.width()).arg(frame.resolution.height()) << ";"
          << frame.resolution.width() << ";"
          << frame.resolution.height() << ";"
          << (frame.resolution.width() * frame.resolution.height()) << "\n";
    }
  }

  // Clean up
  sessionInfo_.clear();
  sessionNames_.clear();
  localInfo_ = {};
}

void StatisticsCSV::addParticipant(uint32_t sessionID, const QString& cname)
{
  sessionNames_[sessionID].append(cname);
}

void StatisticsCSV::removeParticipant(uint32_t sessionID, const QString& cname)
{
  sessionNames_[sessionID].removeAll(cname);
  // Optionally write individual CSV here if you want per-participant mid-call
}

// Ignored
void StatisticsCSV::audioInfo(uint32_t, uint32_t, uint32_t, uint16_t) {}
void StatisticsCSV::videoInfo(uint32_t, uint32_t, double, QSize) {}
void StatisticsCSV::selectedICEPair(uint32_t, std::shared_ptr<ICEPair>) {}
void StatisticsCSV::addSendPacket(uint32_t) {}
void StatisticsCSV::addReceivePacket(uint32_t, const QString&, QString, uint32_t) {}
void StatisticsCSV::addRTCPPacket(uint32_t, const QString&, QString, uint8_t, int32_t, uint32_t, uint32_t) {}

void StatisticsCSV::encodedAudioFrame(uint32_t size, uint32_t encodingTime)
{
  EncodedFrame frame;
  frame.size = size;
  frame.encodingTime = encodingTime;
  localInfo_.encodedAudioFrames.push_back(frame);
}

void StatisticsCSV::encodedVideoFrame(uint32_t size, uint32_t encodingTime, QSize resolution,
                                      float psnrY, float psnrU, float psnrV)
{
  EncodedFrame frame;
  frame.size = size;
  frame.encodingTime = encodingTime;
  frame.resolution = resolution;
  frame.psnrY = psnrY;
  frame.psnrU = psnrU;
  frame.psnrV = psnrV;
  localInfo_.encodedVideoFrames.push_back(frame);
}

void StatisticsCSV::decodedAudioFrame(QString cname, uint32_t size, uint32_t decodingTime)
{
  DecodedFrame frame;
  frame.size = size;
  frame.decodingTime = decodingTime;
  sessionInfo_[cname].decodedAudioFrames.push_back(frame);
}

void StatisticsCSV::decodedVideoFrame(QString cname, uint32_t size, uint32_t decodingTime, QSize resolution)
{
  DecodedFrame frame;
  frame.size = size;
  frame.decodingTime = decodingTime;
  frame.resolution = resolution;
  sessionInfo_[cname].decodedVideoFrames.push_back(frame);
}

void StatisticsCSV::audioLatency(uint32_t, QString cname, int64_t delay)
{
  sessionInfo_[cname].audioLatencies.push_back(delay);
}

void StatisticsCSV::videoLatency(uint32_t, QString cname, int64_t delay)
{
  sessionInfo_[cname].videoLatencies.push_back(delay);
}

uint32_t StatisticsCSV::addFilter(QString type, QString identifier, uint64_t TID)
{
  return 0;
}

void StatisticsCSV::removeFilter(uint32_t id)
{

}

// Tracking of buffer information.
void StatisticsCSV::updateBufferStatus(uint32_t id, uint16_t buffersize,
                                uint16_t maxBufferSize)
{

}

// Tracking of packets dropped due to buffer overflow
void StatisticsCSV::packetDropped(uint32_t id)
{

}

// SIP
// Tracking of sent and received SIP Messages
void StatisticsCSV::addSentSIPMessage(const QString& headerType, const QString& header,
                               const QString& bodyType,   const QString& body)
{

}

void StatisticsCSV::addReceivedSIPMessage(const QString& headerType, const QString& header,
                                   const QString& bodyType,   const QString& body)
{

}
