#include <sipfieldparser.h>

#include <QRegularExpression>
#include <QDebug>


void parseToField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message)
{

}

void parseFromField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message)
{}

void parseCSeqField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message)
{}

void parseCallIDField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message)
{}

void parseViaField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message)
{}

void parseMaxForwardsField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message)
{}

void parseContactField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message)
{}

void parseContentTypeField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message)
{}

void parseContentLengthField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message)
{}


bool isLinePresent(QString name, QList<SIPField>& fields)
{
  for(SIPField field : fields)
  {
    if(field.name == name)
    {
      return true;
    }
  }
  qDebug() << "Did not find header:" << name;
  return false;
}



bool parseParameter(QString text, SIPParameter& parameter)
{
  QRegularExpression re_parameter("([^=]+)=([^;]+)");
  QRegularExpressionMatch parameter_match = re_parameter.match(text);
  if(parameter_match.hasMatch() && parameter_match.lastCapturedIndex() == 2)
  {
    parameter.name = parameter_match.captured(1);
    parameter.value = parameter_match.captured(2);
    return true;
  }

  return false;
}

