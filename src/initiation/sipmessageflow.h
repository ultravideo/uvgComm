#pragma once


#include "initiation/sipmessageprocessor.h"

class SIPDialogState;
class SIPSingleCall;

/* This class represent a collection of processors that can be
 * used as a single processor for simplicity. This removes the need
 * for implementing processor flows in different places multiple times. */

class SIPMessageFlow : public SIPMessageProcessor
{
  Q_OBJECT
public:
  SIPMessageFlow();

  void uninit();

  // Adds processor to the incoming side of flow. The first processor added is
  // the first to process incoming messages and the last to process outgoing messages.
  void addProcessor(std::shared_ptr<SIPMessageProcessor> processor);

public slots:

  virtual void processOutgoingRequest(SIPRequest& request, QVariant& content);
  virtual void processOutgoingResponse(SIPResponse& response, QVariant& content);

  virtual void processIncomingRequest(SIPRequest& request, QVariant& content);
  virtual void processIncomingResponse(SIPResponse& response, QVariant& content);

private:

  bool internalCheck();

  QList<std::shared_ptr<SIPMessageProcessor>> messageFlow_;
};

