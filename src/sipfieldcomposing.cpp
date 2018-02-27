#include "sipfieldcomposing.h"



bool includeToField(QList<SIPField> &fields,
                    std::shared_ptr<SIPMessageInfo> message)
{

  SIPField field = {"To", message->routing->to.realname
                 + " <" + message->routing->to.username
                 + "@" + message->routing->to.host + ">", NULL};

  if(message->session->toTag != "")
  {
    field.parameters = std::shared_ptr<QList<SIPParameter>> (new QList<SIPParameter>);
    field.parameters->append({"tag", message->session->toTag});
  }
  fields.push_back(field);
  return true;
}

bool includeFromField(QList<SIPField> &fields,
                      std::shared_ptr<SIPMessageInfo> message)
{}

bool includeCSeqField(QList<SIPField> &fields,
                      std::shared_ptr<SIPMessageInfo> message)
{}

bool includeCallIDField(QList<SIPField> &fields,
                        std::shared_ptr<SIPMessageInfo> message)
{}

bool includeViaField(QList<SIPField> &fields,
                     std::shared_ptr<SIPMessageInfo> message)
{}

bool includeMaxForwardsField(QList<SIPField> &fields,
                             std::shared_ptr<SIPMessageInfo> message)
{}

bool includeContactField(QList<SIPField> &fields,
                         std::shared_ptr<SIPMessageInfo> message)
{}

bool includeContentTypeField(QList<SIPField> &fields,
                             std::shared_ptr<SIPMessageInfo> message)
{}

bool includeContentLengthField(QList<SIPField> &fields,
                               std::shared_ptr<SIPMessageInfo> message)
{}
