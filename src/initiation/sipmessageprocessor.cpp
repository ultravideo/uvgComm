


#include "sipmessageprocessor.h"



SIPMessageProcessor::SIPMessageProcessor()
{}


SIPMessageProcessor::~SIPMessageProcessor()
{}


void SIPMessageProcessor::connectOutgoingProcessor(SIPMessageProcessor& module)
{
  QObject::connect(this, &SIPMessageProcessor::outgoingRequest,
                   &module, &SIPMessageProcessor::processOutgoingRequest);

  QObject::connect(this, &SIPMessageProcessor::outgoingResponse,
                   &module, &SIPMessageProcessor::processOutgoingResponse);
}


void SIPMessageProcessor::connectIncomingProcessor(SIPMessageProcessor& module)
{
  QObject::connect(this, &SIPMessageProcessor::incomingRequest,
                   &module, &SIPMessageProcessor::processIncomingRequest);

  QObject::connect(this, &SIPMessageProcessor::incomingResponse,
                   &module, &SIPMessageProcessor::processIncomingResponse);
}


void SIPMessageProcessor::processOutgoingRequest(SIPRequest& request, QVariant& content)
{
  // default is that we do no processing
  emit outgoingRequest(request, content);
}


void SIPMessageProcessor::processOutgoingResponse(SIPResponse& response, QVariant& content)
{
  // default is that we do no processing
  emit outgoingResponse(response, content);
}

void SIPMessageProcessor::processIncomingRequest(SIPRequest& request, QVariant& content)
{
  // default is that we do no processing
  emit incomingRequest(request, content);
}


void SIPMessageProcessor::processIncomingResponse(SIPResponse& response, QVariant& content)
{
  // default is that we do no processing
  emit incomingResponse(response, content);
}
