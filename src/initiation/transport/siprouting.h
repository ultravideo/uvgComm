#pragma once


#include "initiation/sipmessageprocessor.h"
#include "initiation/siptypes.h"
#include "initiation/transport/tcpconnection.h"

#include <QString>

#include <memory>

/* This class is responsible for via and contact fields of SIP Requests
 * and possible contact fields of responses */

class SIPRouting : public SIPMessageProcessor
{
  Q_OBJECT
public:
  SIPRouting(std::shared_ptr<TCPConnection> connection);


public slots:

  // add via and contact fields if necessary
  virtual void processOutgoingRequest(SIPRequest& request, QVariant& content);

  // adds contact if necessary
  virtual void processOutgoingResponse(SIPResponse& response, QVariant& content);

  // Checks that we are the correct destination for this response.
  // Also sets our contact address if rport was set.
  virtual void processIncomingResponse(SIPResponse& response, QVariant& content,
                                       bool retryRequest);

private:

  bool isMeantForUs(QList<ViaField> &vias,
                    QString localAddress,
                    uint16_t localPort);

  // check the rport value if
  void updateReceiveAddressAndRport(QList<ViaField> &vias,
                                    QString localAddress,
                                    uint16_t localPort);

  bool isRportDifferent(QList<ViaField> &vias,
                        QString localAddress,
                        uint16_t localPort);

  // sets the via and contact addresses of the request
  void addVia(SIPRequestMethod type, std::shared_ptr<SIPMessageHeader> message,
              QString localAddress, uint16_t localPort);

  // modifies the just the contact-address. Use with responses
  void addContactField(std::shared_ptr<SIPMessageHeader> message,
                          QString localAddress,
                          uint16_t localPort, SIPType type, SIPRequestMethod method);

  void addREGISTERContactParameters(std::shared_ptr<SIPMessageHeader> message);

  void addToSupported(QString feature, std::shared_ptr<SIPMessageHeader> message);

  bool getGruus(std::shared_ptr<SIPMessageHeader> message);

  std::shared_ptr<TCPConnection> connection_;

  QString received_;
  uint16_t rport_;

  QString tempGruu_;
  QString pubGruu_;

  bool resetRegistrations_;

  ViaField previousVia_;
};
