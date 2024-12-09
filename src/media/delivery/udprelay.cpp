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

    // first figure out if this is and RTP packet or an RTCP packet
    uint8_t pt = *reinterpret_cast<uint8_t*>(datagram.data() + 1);

    if (pt < 128)
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
                                        {"SSRC", "Payload type"},
                                        {QString::number(ssrc), QString::number(pt)});
      }
    }
    else
    {
      // RTCP
      Logger::getLogger()->printDebug(DEBUG_WARNING, this, "Received a packet which is not implemented yet",
                                      {"Payload type", " Possible SSRC"},
                                      {QString::number(pt), QString::number(*reinterpret_cast<uint32_t*>(datagram.data() + 4))});

    }
  }
}
