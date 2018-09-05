#pragma once

#include <QStringList>

#include "sip/sdptypes.h"

// This class has the capability to tell what SDP parameters are suitable for us.




class SDPParameterManager
{
public:
  SDPParameterManager();

  // use these if you want all the supported preset RTP payload type codecs ( 1..34 )
  QList<uint8_t> audioPayloadTypes();
  QList<uint8_t> videoPayloadTypes();

  // get supported codecs with dynamic RTP payload types ( 96..127 )
  QList<RTPMap> audioCodecs() const;
  QList<RTPMap> videoCodecs() const;

  QString sessionName() const;
  QString sessionDescription() const;



private:



};
