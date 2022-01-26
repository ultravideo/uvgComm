#include "sipmessageflow.h"

#include "initiation/transaction/sipdialogstate.h"

#include "common.h"
#include "logger.h"

SIPMessageFlow::SIPMessageFlow():
  messageFlow_()
{}


void SIPMessageFlow::uninit()
{
  for (auto& processor : messageFlow_)
  {
    processor->uninit();
  }

  messageFlow_.clear();
}


void SIPMessageFlow::addProcessor(std::shared_ptr<SIPMessageProcessor> processor)
{
  if (processor == nullptr)
  {
    Logger::getLogger()->printProgramError(this, "Processor given as nullptr");
    return;
  }

  Logger::getLogger()->printNormal(this, "Adding processor to flow", "Processor number",
              QString::number(messageFlow_.size() + 1));

  // if there are already processors in the flow, connect to the end
  if (!messageFlow_.empty())
  {

    // disconnect the previously last processor from flow incoming.
    disconnect(messageFlow_.last().get(), &SIPMessageProcessor::incomingRequest,
               this,                      &SIPMessageProcessor::incomingRequest);
    disconnect(messageFlow_.last().get(), &SIPMessageProcessor::incomingResponse,
               this,                      &SIPMessageProcessor::incomingResponse);

    processor->connectOutgoingProcessor(*messageFlow_.last());
    messageFlow_.last()->connectIncomingProcessor(*processor);
  }
  else
  {
    // connect outgoing side to flow if this is the first processor
    connect(processor.get(), &SIPMessageProcessor::outgoingRequest,
            this,          &SIPMessageProcessor::outgoingRequest);
    connect(processor.get(), &SIPMessageProcessor::outgoingResponse,
            this,          &SIPMessageProcessor::outgoingResponse);
  }

  // connect incoming side to this because this is the newest processor
  connect(processor.get(), &SIPMessageProcessor::incomingRequest,
          this,            &SIPMessageProcessor::incomingRequest);
  connect(processor.get(), &SIPMessageProcessor::incomingResponse,
          this,            &SIPMessageProcessor::incomingResponse);

  messageFlow_.push_back(processor);


}


void SIPMessageFlow::processOutgoingRequest(SIPRequest& request, QVariant& content)
{
  if (!internalCheck())
  {
    return;
  }

  messageFlow_.back()->processOutgoingRequest(request, content);
}


void SIPMessageFlow::processOutgoingResponse(SIPResponse& response, QVariant& content)
{
  if (!internalCheck())
  {
    return;
  }

  messageFlow_.back()->processOutgoingResponse(response, content);
}


void SIPMessageFlow::processIncomingRequest(SIPRequest& request, QVariant& content)
{
  if (!internalCheck())
  {
    return;
  }

  messageFlow_.first()->processIncomingRequest(request, content);
}


void SIPMessageFlow::processIncomingResponse(SIPResponse& response, QVariant& content)
{
  if (!internalCheck())
  {
    return;
  }

  messageFlow_.first()->processIncomingResponse(response, content);
}


bool SIPMessageFlow::internalCheck()
{
  if (messageFlow_.empty())
  {
    Logger::getLogger()->printProgramError(this, "Being used while flow not initialized. "
                            "Please call addProcessor function!");
    return false;
  }
  return true;
}
