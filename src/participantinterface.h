#pragma once
#include <QString>

class ParticipantInterface
{
 public:

  virtual ~ParticipantInterface(){}

  // user wants to call to this participant. Returns sessionID
  virtual uint32_t callToParticipant(QString name, QString username, QString address) = 0;

  //user wants to chat with this participant. Returns sessionID
  virtual uint32_t chatWithParticipant(QString name, QString username, QString address) = 0;
};
