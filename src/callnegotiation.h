#pragma once



#include "connectionserver.h"
#include "connection.h"
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

class CallNegotiation : public QObject
{
  Q_OBJECT
public:
  CallNegotiation();
  ~CallNegotiation();

  void init();
  void uninit();

  void setupSession(MediaSubsession* subsession);

  void startCall(QList<Contact> addresses, QString sdp);

  void acceptCall(QString CallID);

  void endCall();

  private slots:

  void receiveConnection(Connection* con);

private:

  void initUs();

  struct SIPLink
  {
    QString callID; // for identification

    Contact peer;
    //uint16_t port;
    uint32_t cseq;

    QString ourTag;
    QString theirTag;

    QString theirName;
    QString theirUsername;

    QString sdp; // current session description

    uint32_t connectionID;
  };

  // helper function that composes SIP message and sends it
  void sendRequest(MessageType request, std::shared_ptr<SIPLink> contact, QString &branch);

  std::map<QString, std::shared_ptr<SIPLink>> sessions_;
  std::vector<Connection*> connections_;

  SIPStringComposer messageComposer_;

  uint16_t sipPort_;

  QString ourName_;
  QString ourUsername_;
  QHostAddress ourLocation_;


  ConnectionServer server_;
};
