#pragma once

#include "initiation/sipmessageprocessor.h"

#include "initiation/siptypes.h"

class SIPAuthentication : public SIPMessageProcessor
{
  Q_OBJECT
public:
  SIPAuthentication();

public slots:

  // add credentials to request, if we have them
  virtual void processOutgoingRequest(SIPRequest& request, QVariant& content);

  // take challenge if they require authentication
  virtual void processIncomingResponse(SIPResponse& response, QVariant& content);


private:

  DigestResponse generateAuthResponse(DigestChallenge& challenge, QString username,
                                      SIP_URI& requestURI, SIPRequestMethod method,
                                      QVariant& content);

  void updateNonceCount(DigestChallenge& challenge, DigestResponse& response);

  QList<DigestChallenge> challenges_;



  // key is realm and value current nonce
  std::map<QString, QString> realmToNonce_;
  std::map<QString, uint32_t> realmToNonceCount_;

  QByteArray a1_;
};
