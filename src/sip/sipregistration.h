#pragma once

#include "sip/siptypes.h"

#include <QHostAddress>

/* This class is responsible for holding necessary SIP information
 * outside of dialog.
*/


class SIPRegistration
{
public:
  SIPRegistration(QString serverAddress);

  void initServer(quint32 transportID);

  //void updateAddressBook();

  std::shared_ptr<SIPMessageInfo> fillRegisterRequest(QString localAddress);

  bool isContactAtThisServer(QString serverAddress);

  quint32 getTransportID()
  {
    Q_ASSERT(transportID_);
    return transportID_;
  }

private:

  void initLocalURI();
  ViaInfo getLocalVia(QString localAddress);

  SIP_URI localUri_;
  QString serverAddress_;

  uint32_t registerCSeq_;

  quint32 transportID_;
};
