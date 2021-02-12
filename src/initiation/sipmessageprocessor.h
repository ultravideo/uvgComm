#pragma once


#include "initiation/siptypes.h"

#include <QObject>

// The base class for one processor in SIP message processing flow.
// Inherit this and reimplement desired functions.

class SIPMessageProcessor : public QObject
{
  Q_OBJECT
public:
  SIPMessageProcessor();
  ~SIPMessageProcessor();

  virtual void uninit();

  // connects the signals
  void connectOutgoingProcessor(SIPMessageProcessor& module);
  void connectIncomingProcessor(SIPMessageProcessor& module);

signals:

  // Emit these signals when you want some other processor to continue
  // processing the message.

  // Do not redefine signals in children!
  virtual void outgoingRequest(SIPRequest& request, QVariant& content);
  virtual void outgoingResponse(SIPResponse& response, QVariant& content);

  virtual void incomingRequest(SIPRequest& request, QVariant& content);
  virtual void incomingResponse(SIPResponse& response, QVariant& content);

public slots:

  // The actual processing is done in these slots. Reimplement these if you
  // want to do processing on that specific type of message.

  virtual void processOutgoingRequest(SIPRequest& request, QVariant& content);
  virtual void processOutgoingResponse(SIPResponse& response, QVariant& content);

  virtual void processIncomingRequest(SIPRequest& request, QVariant& content);
  virtual void processIncomingResponse(SIPResponse& response, QVariant& content);

};
