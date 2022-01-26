#pragma once


#include "initiation/siptypes.h"

#include <QObject>

/* The base class for one processor in SIP message processing flow.
 * Inherit this and reimplement desired functions. The default
 * implementation simply passes the messages forward. You should
 * probably remember to do this in your own implementations as well.
 */

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

  // Do NOT redefine signals in children!
  void outgoingRequest(SIPRequest& request, QVariant& content);
  void outgoingResponse(SIPResponse& response, QVariant& content);

  // TODO: have some sort of error state attached to incoming requests
  void incomingRequest(SIPRequest& request, QVariant& content);
  void incomingResponse(SIPResponse& response, QVariant& content);

public slots:

  // The actual processing is done in these slots. Reimplement these if you
  // want to do processing on that specific type of message.

  virtual void processOutgoingRequest(SIPRequest& request, QVariant& content);
  virtual void processOutgoingResponse(SIPResponse& response, QVariant& content);

  virtual void processIncomingRequest(SIPRequest& request, QVariant& content);
  virtual void processIncomingResponse(SIPResponse& response, QVariant& content);

};
