#pragma once
#include <QString>

class ParticipantInterface
{
 public:

  virtual ~ParticipantInterface(){}

  // user wants to call to this participant
  virtual void callToParticipant(QString name, QString username, QString address) = 0;

  //user wants to chat with this participant
  virtual void chatWithParticipant(QString name, QString username, QString address) = 0;
};
