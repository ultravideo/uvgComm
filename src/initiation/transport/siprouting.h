#pragma once


#include "initiation/sipmessageprocessor.h"
#include "initiation/siptypes.h"
#include "initiation/transport/tcpconnection.h"

#include <QString>

#include <memory>

// This class is responsible for via and contact fields of SIP Requests
// and possible contact fields of responses

class SIPRouting : public SIPMessageProcessor
{
public:
  SIPRouting(std::shared_ptr<TCPConnection> connection);


public slots:

  // add via and contact fields if necessary
  virtual void processOutgoingRequest(SIPRequest& request, QVariant& content);

  // adds contact if necessary
  virtual void processOutgoingResponse(SIPResponse& response, QVariant& content);

  // Checks that we are the correct destination for this response.
  // Also sets our contact address if rport was set.
  virtual void processIncomingResponse(SIPResponse& response, QVariant& content);


  // TODO: Should we also check the incoming request. Test with server

private:

  // check the rport value if
  void processResponseViaFields(QList<ViaField> &vias,
                                QString localAddress,
                                uint16_t localPort);

  // sets the via and contact addresses of the request
  void addVia(SIPRequestMethod type, std::shared_ptr<SIPMessageHeader> message,
                         QString localAddress,
                         uint16_t localPort);

  // modifies the just the contact-address. Use with responses
  void addContactField(std::shared_ptr<SIPMessageHeader> message,
                          QString localAddress,
                          uint16_t localPort, SIPType type);


  std::shared_ptr<TCPConnection> connection_;

  QString contactAddress_;
  uint16_t contactPort_;

  bool first_;

  ViaField previousVia_;
};
