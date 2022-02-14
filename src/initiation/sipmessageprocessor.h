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

  // generated response parameter is for situations when the response is decided
  // by components before server
  void incomingRequest(SIPRequest& request, QVariant& content,
                       SIPResponseStatus generatedResponse);

  // retry parameter indicates whether a component needs the same request sent
  // again to function correctly
  void incomingResponse(SIPResponse& response, QVariant& content,
                        bool retryRequest);

public slots:

  // The actual processing is done in these slots. Reimplement these if you
  // want to do processing on that specific type of message.

  virtual void processOutgoingRequest(SIPRequest& request, QVariant& content);
  virtual void processOutgoingResponse(SIPResponse& response, QVariant& content);

  virtual void processIncomingRequest(SIPRequest& request, QVariant& content,
                                      SIPResponseStatus generatedResponse);
  virtual void processIncomingResponse(SIPResponse& response, QVariant& content,
                                       bool retryRequest);

};
