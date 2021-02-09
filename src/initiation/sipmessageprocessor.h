#pragma once


#include "initiation/siptypes.h"

#include <QObject>


class SIPMessageProcessor : public QObject
{
  Q_OBJECT
public:
  SIPMessageProcessor();
  ~SIPMessageProcessor();

  virtual void uninit();

  void connectOutgoingProcessor(SIPMessageProcessor& module);
  void connectIncomingProcessor(SIPMessageProcessor& module);

signals:
  void outgoingRequest(SIPRequest& request, QVariant& content);
  void outgoingResponse(SIPResponse& response, QVariant& content);

  void incomingRequest(SIPRequest& request, QVariant& content);
  void incomingResponse(SIPResponse& response, QVariant& content);

public slots:

  virtual void processOutgoingRequest(SIPRequest& request, QVariant& content);
  virtual void processOutgoingResponse(SIPResponse& response, QVariant& content);

  virtual void processIncomingRequest(SIPRequest& request, QVariant& content);
  virtual void processIncomingResponse(SIPResponse& response, QVariant& content);

};
