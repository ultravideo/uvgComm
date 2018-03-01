#include "sipfieldcomposing.h"

#include "sipconversions.h"

#include <QDebug>


// a function used within this file to add a parameter
bool tryAddParameter(SIPField& field, QString parameterName, QString parameterValue);


bool getFirstRequestLine(QString& line, SIPRequest& request, QString lineEnding)
{
  if(request.type == SIP_UNKNOWN_REQUEST)
  {
    qDebug() << "WARNING: First request line failed";
    return false;
  }
  line = requestToString(request.type) + " "
      + request.message->routing->to.username + "@" + request.message->routing->to.host
      + " SIP/" + request.message->version + lineEnding;
  return true;
}

bool getFirstResponseLine(QString& line, SIPResponse& response, QString lineEnding)
{
  if(response.type == SIP_UNKNOWN_RESPONSE)
  {
    qDebug() << "WARNING: First response line failed";
    return false;
  }
  line = " SIP/" + response.message->version
      + QString::number(responseToCode(response.type)) + " "
      + responseToPhrase(response.type) + lineEnding;
  return true;
}

bool includeToField(QList<SIPField> &fields,
                    std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message->routing->to.username != "" && message->routing->to.host != "");
  if(message->routing->to.username == "" ||  message->routing->to.host == "")
  {
    qDebug() << "WARNING: To field failed";
    return false;
  }

  SIPField field;
  field.name = "To";
  if(message->routing->to.realname != "")
  {
    field.values = message->routing->to.realname + " ";
  }
  field.values += "<sip:" + message->routing->to.username + "@" + message->routing->to.host + ">";
  field.parameters = NULL;

  tryAddParameter(field, "tag", message->session->toTag);

  fields.push_back(field);
  return true;
}

bool includeFromField(QList<SIPField> &fields,
                      std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message->routing->from.username != "" && message->routing->from.host != "");
  if(message->routing->from.username == "" ||  message->routing->from.host == "")
  {
    qDebug() << "WARNING: From field failed";
    return false;
  }

  SIPField field;
  field.name = "From";
  if(message->routing->from.realname != "")
  {
    field.values = message->routing->from.realname + " ";
  }
  field.values += "<sip:" + message->routing->from.username + "@" + message->routing->from.host + ">";
  field.parameters = NULL;

  tryAddParameter(field, "tag", message->session->fromTag);

  fields.push_back(field);
  return true;
}

bool includeCSeqField(QList<SIPField> &fields,
                      std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message->cSeq != 0 && message->transactionRequest != SIP_UNKNOWN_REQUEST);
  if(message->cSeq == 0 || message->transactionRequest == SIP_UNKNOWN_REQUEST)
  {
    qDebug() << "WARNING: cSeq field failed";
    return false;
  }

  SIPField field;
  field.name = "CSeq";

  field.values = QString::number(message->cSeq) + " " + requestToString(message->transactionRequest);
  field.parameters = NULL;

  fields.push_back(field);
  return true;
}

bool includeCallIDField(QList<SIPField> &fields,
                        std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message->session->callID != "");
  if(message->session->callID == "")
  {
    qDebug() << "WARNING: Call-ID field failed";
    return false;
  }

  fields.push_back({"Call-ID", message->session->callID, NULL});
  return true;
}

bool includeViaFields(QList<SIPField> &fields,
                      std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(!message->routing->senderReplyAddress.empty());
  if(message->routing->senderReplyAddress.empty())
  {
    qDebug() << "WARNING: Via field failed";
    return false;
  }

  for(ViaInfo via : message->routing->senderReplyAddress)
  {
    Q_ASSERT(via.type != ANY);
    Q_ASSERT(via.branch != "");

    SIPField field;
    field.name = "Via";

    field.values = "SIP/" + via.version +"/" + connectionToString(via.type) + " " + via.address;

    if(!tryAddParameter(field, "branch", via.branch))
    {
      qDebug() << "WARNING: Via field failed";
      return false;
    }
    fields.push_back(field);
  }
  return true;
}

bool includeMaxForwardsField(QList<SIPField> &fields,
                             std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message->routing->maxForwards != 0);
  if(message->routing->maxForwards == 0)
  {
    qDebug() << "WARNING: Max-forwards field failed";
    return false;
  }

  fields.push_back({"Max-Forwards", QString::number(message->routing->maxForwards), NULL});
  return true;
}

bool includeContactField(QList<SIPField> &fields,
                         std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message->routing->contact.username != "" && message->routing->contact.host != "");
  if(message->routing->contact.username == "" ||  message->routing->contact.host == "")
  {
    qDebug() << "WARNING: Contact field failed";
    return false;
  }

  SIPField field;
  field.name = "Contact";
  field.values = "<sip:" + message->routing->contact.username + "@" + message->routing->contact.host + ">";
  field.parameters = NULL;
  fields.push_back(field);
  return true;
}

bool includeContentTypeField(QList<SIPField> &fields,
                             QString contentType)
{
  Q_ASSERT(contentType != "");
  if(contentType == "")
  {
    qDebug() << "WARNING: Content-type field failed";
    return false;
  }
  fields.push_back({"Content-Type", contentType, NULL});
  return true;
}

bool includeContentLengthField(QList<SIPField> &fields,
                               uint32_t contentLenght)
{
  fields.push_back({"Content-Length", QString::number(contentLenght), NULL});
  return true;
}

bool tryAddParameter(SIPField& field, QString parameterName, QString parameterValue)
{
  if(parameterValue == "")
  {
    return false;
  }

  if(field.parameters == NULL)
  {
    field.parameters = std::shared_ptr<QList<SIPParameter>> (new QList<SIPParameter>);
  }
  field.parameters->append({parameterName, parameterValue});

  return true;
}
