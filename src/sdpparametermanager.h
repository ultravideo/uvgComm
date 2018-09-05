#pragma once

#include <QStringList>

#include "sip/sdptypes.h"

// This class has the capability to tell what SDP parameters are suitable for us.


class SDPParameterManager
{
public:
  SDPParameterManager();

  QStringList audioCodecs() const;
  QStringList videoCodecs() const;

  QString sessionName() const;
  QString sessionDescription() const;


private:

};
