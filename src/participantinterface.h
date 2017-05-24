#pragma once

#include <QString>


class ParticipantInterface
{
   public:
     virtual ~ParticipantInterface(){} // do not forget this

public slots:
  virtual void callToParticipant(QString name, QString username, QString address) = 0;
  virtual void chatWithParticipant(QString name, QString username, QString address) = 0;
};

Q_DECLARE_INTERFACE(ParticipantInterface, "ParticipantInterface") // define this out of namespace scope
