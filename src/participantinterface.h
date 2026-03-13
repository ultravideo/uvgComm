#pragma once
#include <QString>
#include <QObject>

class ParticipantInterface : public QObject
{
  Q_OBJECT
 public:

  virtual ~ParticipantInterface(){}

  // user wants to call to this participant. Returns sessionID
  virtual uint32_t startINVITETransaction(QString name, QString username, QString address) = 0;

  //user wants to chat with this participant. Returns sessionID
  virtual uint32_t chatWithParticipant(QString name, QString username, QString address) = 0;
};
