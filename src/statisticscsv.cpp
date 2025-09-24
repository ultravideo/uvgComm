#include "statisticscsv.h"

#include "logger.h"
#include "cname.h"

#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QDateTime>
#include <QDebug>

const QString baseFolder = "stats_csv";

StatisticsCSV::StatisticsCSV()
{
  // Create output directory
  QDir().mkpath(baseFolder);
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
    out << "Frame;Type;Size(Bytes);DecodeTime(ms);Latency(ms);Resolution;Width;Height;Pixels\n";

    const auto& info = sessionInfo_[cname];
    unsigned int frame_number = 1;

    // Video frames
    for (int i = 0; i < info.decodedVideoFrames.size(); ++i)
    {
      const auto& frame = info.decodedVideoFrames[i];
      QString latencyString = "N/A";
      if (info.videoLatencies.find(frame.timestamp) != info.videoLatencies.end())
      {
        if (info.videoLatencies.at(frame.timestamp) >= 0)
        {
          latencyString = QString::number(info.videoLatencies.at(frame.timestamp));
        }
        else
        {
          latencyString = "-1";
        }
      }

      int width = frame.resolution.width();
      int height = frame.resolution.height();
      int pixels = width * height;

      out << frame_number << ";Video;"
          << frame.size << ";"
          << frame.decodingTime << ";"
          << latencyString << ";"
          << QString("%1x%2").arg(width).arg(height) << ";"
          << width << ";"
          << height << ";"
          << pixels << "\n";

      frame_number++;
    }

    // Audio frames
    for (int i = 0; i < info.decodedAudioFrames.size(); ++i)
    {
      const auto& frame = info.decodedAudioFrames[i];
      QString latencyString = "";
      if (info.audioLatencies.find(frame.timestamp) != info.audioLatencies.end())
      {
        if (info.audioLatencies.at(frame.timestamp) >= 0)
        {
          latencyString = QString::number(info.audioLatencies.at(frame.timestamp));
        }
        else
        {
          latencyString = "-1";
        }
      }

      out << frame_number << ";Audio;"
          << frame.size << ";"
          << frame.decodingTime << ";"
          << latencyString << ";"
          << ";;;;" << "\n"; // no resolution for audio

      frame_number++;
    }
  }

  QString localCname = CName::cname();

  // Save local info (only once per session)
  QString filename = sessionFolder + QString("/local_" + CName::cname() + QString(".csv"));
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

void StatisticsCSV::decodedAudioFrame(QString cname, int64_t timestamp, uint32_t size, uint32_t decodingTime)
{
  DecodedFrame frame;
  frame.size = size;
  frame.decodingTime = decodingTime;
  frame.timestamp = timestamp;
  sessionInfo_[cname].decodedAudioFrames.push_back(frame);
}

void StatisticsCSV::decodedVideoFrame(QString cname, int64_t timestamp, uint32_t size, uint32_t decodingTime, QSize resolution)
{
  DecodedFrame frame;
  frame.size = size;
  frame.decodingTime = decodingTime;
  frame.resolution = resolution;
  frame.timestamp = timestamp;
  sessionInfo_[cname].decodedVideoFrames.push_back(frame);
}

void StatisticsCSV::audioLatency(uint32_t, QString cname, int64_t timestamp, int64_t delay)
{
  sessionInfo_[cname].audioLatencies[timestamp] = delay;
}

void StatisticsCSV::videoLatency(uint32_t, QString cname, int64_t timestamp, int64_t delay)
{
  sessionInfo_[cname].videoLatencies[timestamp] = delay;
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
