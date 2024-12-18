#include "udprelay.h"

#include "src/media/processing/filter.h"

#include "logger.h"

#include <QUdpSocket>
#include <QHostAddress>
#include <QDateTime>


UDPRelay::UDPRelay(std::string localAddress, uint16_t port)
{
  socket_.bind(QHostAddress(localAddress.c_str()), port);
  connect(&socket_, &QUdpSocket::readyRead, this, &UDPRelay::readPendingDatagrams);
}


void UDPRelay::registerRTPReceiver(uint32_t ssrc, std::shared_ptr<Filter> filter)
{
  receivers_[ssrc] = filter;
}


void UDPRelay::sendUDPData(std::string destinationAddress, uint16_t port,
                           std::unique_ptr<unsigned char[]> data, uint32_t size)
{
  socket_.writeDatagram(reinterpret_cast<char*>(data.get()), size,
                        QHostAddress(destinationAddress.c_str()), port);
}


void UDPRelay::readPendingDatagrams()
{
  while (socket_.hasPendingDatagrams())
  {
    QByteArray datagram;
    datagram.resize(socket_.pendingDatagramSize());
    QHostAddress sender;
    quint16 senderPort;

    socket_.readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);

    if (datagram.size() < 12)
    {
      //Logger::getLogger()->printDebug(DEBUG_WARNING, this, "Received a packet which is too small",
      //                                {"Packet size"}, {QString::number(datagram.size())});
      continue;
    }

    Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Received a packet",
                                    {"Packet size", "Sender IP", "Sender port"},
                                    {QString::number(datagram.size()), sender.toString(), QString::number(senderPort)});

    // first figure out if this is and RTP packet or an RTCP packet
    uint8_t rtcp_pt = *reinterpret_cast<uint8_t*>(datagram.data() + 1);
    uint8_t rtp_pt = rtcp_pt & 0x7F;

    if (rtcp_pt == 200 || rtcp_pt == 201 || rtcp_pt == 202)
    {
      // RTCP
      Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Received an RTCP packet, not doing anything",
                                      {"Payload type", "Packet size"},
                                      {QString::number(rtcp_pt), QString::number(datagram.size())});
    }
    else if (rtp_pt <= 34 || 96 <= rtp_pt)
    {
      // RTP
      uint32_t ssrc = *reinterpret_cast<uint32_t*>(datagram.data() + 8);
      ssrc = htonl(ssrc);

      if (receivers_.find(ssrc) != receivers_.end())
      {
        std::shared_ptr<Filter> filter = receivers_[ssrc];

        std::unique_ptr<Data> receivedRTPFrame = Filter::initializeData(DT_RTP, DS_REMOTE);
        receivedRTPFrame->creationTimestamp = QDateTime::currentMSecsSinceEpoch();
        receivedRTPFrame->presentationTimestamp = receivedRTPFrame->creationTimestamp;

        receivedRTPFrame->data = std::unique_ptr<uchar[]>(new uchar[datagram.size()]);
        memcpy(receivedRTPFrame->data.get(), datagram.data(), datagram.size());
        receivedRTPFrame->data_size = datagram.size();

        filter->putInput(std::move(receivedRTPFrame));
      }
      else
      {
        Logger::getLogger()->printDebug(DEBUG_WARNING, this, "Received an RTP packet for which we have no receiver",
                                        {"SSRC", "Payload type", "Packet size"},
                                        {QString::number(ssrc), QString::number(rtp_pt), QString::number(datagram.size())});
      }
    }
    else
    {
      // unknown payload type
      Logger::getLogger()->printDebug(DEBUG_WARNING, this, "Received a packet which does not follow RTP specifications",
                                      {"Payload type", " Possible SSRC", "Packet size"},
                                      {QString::number(rtp_pt),
                                       QString::number(*reinterpret_cast<uint32_t*>(datagram.data() + 4)),
                                       QString::number(datagram.size())});

    }
  }
}
