#pragma once
#include <QString>

class ParticipantInterface
{
 public:

  virtual ~ParticipantInterface(){}

  virtual void callToParticipant(QString name, QString username, QString address) = 0;
  virtual void chatWithParticipant(QString name, QString username, QString address) = 0;
};
