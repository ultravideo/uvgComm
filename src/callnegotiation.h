#pragma once

#include <MediaSession.hh>

#include <QHostAddress>
#include <QString>

class CallNegotiation
{
public:
  CallNegotiation();
  ~CallNegotiation();

  void init();

  void setupSession(MediaSubsession* subsession);

  void startCall(QList<QHostAddress> addresses);

  void acceptCall(QString CallID);

  void endCall();

private:

  struct SIPLink
  {
    QString CallID; // for identification

    QString name;
    QHostAddress address;

    uint16_t port;

    uint32_t cseq;

    QString ourTag;
    QString theirTag;

    QString sdp; // current session description
  };
};
