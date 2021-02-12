#pragma once


#include "initiation/sipmessageprocessor.h"

class SIPDialogState;
class SIPSingleCall;

// The message flow class also functions as

class SIPMessageFlow : public SIPMessageProcessor
{
  Q_OBJECT
public:
  SIPMessageFlow();

  void uninit();

  // dialogstate is always the first to process incoming messages.
  // You should always add dialogstate and at least one processor
  // before sending messages.
  void addDialogState(std::shared_ptr<SIPDialogState> dialog);

  // adds processor to the incoming side of flow
  void addProcessor(std::shared_ptr<SIPMessageProcessor> processor);

  bool isRequestForYou(QString callID, QString toTag, QString fromTag);
  bool isResponseForYou(QString callID, QString toTag, QString fromTag);

public slots:

  virtual void processOutgoingRequest(SIPRequest& request, QVariant& content);
  virtual void processOutgoingResponse(SIPResponse& response, QVariant& content);

  virtual void processIncomingRequest(SIPRequest& request, QVariant& content);
  virtual void processIncomingResponse(SIPResponse& response, QVariant& content);

private:

  bool internalCheck();

  QList<std::shared_ptr<SIPMessageProcessor>> messageFlow_;

  std::shared_ptr<SIPDialogState> dialog_;
};

