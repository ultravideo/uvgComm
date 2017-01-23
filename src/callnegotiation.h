#pragma once


#include "networkreceiver.h"
#include "networksender.h"

#include <MediaSession.hh>

#include <QHostAddress>
#include <QString>

#include <memory>

struct Contact
{
  QHostAddress address;
  QString name;
};

class CallNegotiation
{
public:
  CallNegotiation();
  ~CallNegotiation();

  void init();

  void setupSession(MediaSubsession* subsession);

  void startCall(QList<Contact> addresses, QString sdp);

  void acceptCall(QString CallID);

  void endCall();

private:

  struct SIPLink
  {
    QString callID; // for identification

    Contact peer;

    uint16_t port;

    uint32_t cseq;

    QString ourTag;
    QString theirTag;

    QString sdp; // current session description

    NetworkSender sender;
  };


  std::map<QString, std::unique_ptr<SIPLink>> negotiations_;

  NetworkReceiver receiver;




};
