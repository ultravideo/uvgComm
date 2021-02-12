#include "sipmessageflow.h"

#include "initiation/transaction/sipdialogstate.h"

#include "common.h"

SIPMessageFlow::SIPMessageFlow():
  messageFlow_(),
  dialog_(nullptr)
{

}


void SIPMessageFlow::uninit()
{
  if (dialog_)
  {
    dialog_->uninit();
  }

  for (auto& processor : messageFlow_)
  {
    processor->uninit();
  }

  dialog_ = nullptr;

  messageFlow_.clear();
}


void SIPMessageFlow::addDialogState(std::shared_ptr<SIPDialogState> dialog)
{
  if (dialog == nullptr)
  {
    printProgramError(this, "Dialog state as nullptr");
    return;
  }

  printNormal(this, "Adding dialog to flow");

  dialog_ = dialog;

  connect(dialog_.get(), &SIPMessageProcessor::outgoingRequest,
          this,          &SIPMessageProcessor::outgoingRequest);
  connect(dialog_.get(), &SIPMessageProcessor::outgoingResponse,
          this,          &SIPMessageProcessor::outgoingResponse);

  // connect dialog to pipe if it has members
  if (!messageFlow_.empty())
  {
    dialog_->connectIncomingProcessor(*messageFlow_.first());
    messageFlow_.first()->connectOutgoingProcessor(*dialog_);
  }
}


void SIPMessageFlow::addProcessor(std::shared_ptr<SIPMessageProcessor> processor)
{
  if (processor == nullptr)
  {
    printProgramError(this, "Processor given as nullptr");
    return;
  }

  printNormal(this, "Adding processor to flow", "Processor number",
              QString::number(messageFlow_.size() + 1));

  // if dialog is already here and this is the first processor added after it
  if (dialog_ != nullptr && messageFlow_.empty())
  {
    dialog_->connectIncomingProcessor(*processor);
    processor->connectOutgoingProcessor(*dialog_);
  }

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

  connect(processor.get(), &SIPMessageProcessor::incomingRequest,
          this,            &SIPMessageProcessor::incomingRequest);
  connect(processor.get(), &SIPMessageProcessor::incomingResponse,
          this,            &SIPMessageProcessor::incomingResponse);

  messageFlow_.push_back(processor);


}


bool SIPMessageFlow::isRequestForYou(QString callID, QString toTag, QString fromTag)
{
  if (!internalCheck())
  {
    return false;
  }

  return dialog_->correctRequestDialog(callID, toTag, fromTag);
}


bool SIPMessageFlow::isResponseForYou(QString callID, QString toTag, QString fromTag)
{
  if (!internalCheck())
  {
    return false;
  }
  return dialog_->correctResponseDialog(callID, toTag, fromTag);
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

  dialog_->processIncomingRequest(request, content);
}


void SIPMessageFlow::processIncomingResponse(SIPResponse& response, QVariant& content)
{
  if (!internalCheck())
  {
    return;
  }

  dialog_->processIncomingResponse(response, content);
}


bool SIPMessageFlow::internalCheck()
{
  if (dialog_ == nullptr || messageFlow_.empty())
  {
    printProgramError(this, "Being used while flow not initialized. "
                            "Please call addProcessor and addDialogState functions");
    return false;
  }
  return true;
}
