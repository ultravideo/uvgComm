#pragma once


#include "networkreceiver.h"
#include "networksender.h"
#include "sipstringcomposer.h"

#include <MediaSession.hh>

#include <QHostAddress>
#include <QString>

#include <memory>

struct Contact
{
  QHostAddress address;
  QHostInfo hostname;
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
    //uint16_t port;
    uint32_t cseq;

    QString ourTag;
    QString theirTag;

    // stored for convenience
    QString ourName;
    QString theirName;

    QString sdp; // current session description

    NetworkSender sender;
  };

  // helper function that composes SIP message and sends it
  void sendRequest(Request request, std::shared_ptr<SIPLink> contact);

  std::map<QString, std::shared_ptr<SIPLink>> sessions_;

  NetworkReceiver receiver_;

  SIPStringComposer messageComposer_;

  uint16_t sipPort_;
};
