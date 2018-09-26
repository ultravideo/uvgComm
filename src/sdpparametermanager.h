#pragma once

#include "sip/sdptypes.h"

#include <QStringList>
#include <QMutex>

#include <deque>

// This class has the capability to tell what SDP parameters are suitable for us.

class SDPParameterManager
{
public:
  SDPParameterManager();

  void setPortRange(uint16_t minport, uint16_t maxport, uint16_t maxRTPConnections);

  // use these if you want all the supported preset RTP payload type codecs ( 1..34 )
  QList<uint8_t> audioPayloadTypes();
  QList<uint8_t> videoPayloadTypes();

  // get supported codecs with dynamic RTP payload types ( 96..127 )
  QList<RTPMap> audioCodecs() const;
  QList<RTPMap> videoCodecs() const;

  QString sessionName() const;
  QString sessionDescription() const;

  bool enoughFreePorts()
  {
    return remainingPorts_ >= 4;
  }

  // return the lower port of the pair and removes both from list of available ports
  uint16_t nextAvailablePortPair();
  void makePortPairAvailable(uint16_t lowerPort);

private:



  uint16_t remainingPorts_;

  QMutex portLock_;
  //keeps a list of all available ports. Has only every other port because of rtcp
  std::deque<uint16_t> availablePorts_;

};
