#pragma once



#include "connectionserver.h"
#include "connection.h"
#include "sipstringcomposer.h"
#include "common.h"

#include <MediaSession.hh>

#include <QHostAddress>
#include <QString>

#include <memory>

struct SIPMessageInfo;


struct Contact
{
  QString name;
  QString username;
  QString contactAddress;
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
  void ringing(QString CallID);
  void acceptCall(QString CallID);
  void rejectCall(QString CallID);
  void endCall();

signals:

  // somebody wants to call us. Ask user whether this person is ok
  void incomingINVITE(QString CallID, QString caller);
  void callingOurselves();


private slots:
  void receiveConnection(Connection* con);
  void processMessage(QString header, QString content, quint32 connectionID);

  void connectionEstablished(quint32 connectionID);

private:

  struct SIPLink
  {
    QString callID; // for identification
    QString host;

    Contact contact;

    QString replyAddress;
    uint32_t cSeq;

    QString ourTag;
    QString theirTag;
    QHostAddress localAddress;

    QString sdp; // current session description

    uint32_t connectionID;

    MessageType sentRequest;

    bool ourselves;
  };



  void initUs();

  std::shared_ptr<SIPLink> newSIPLink();
  bool compareSIPLinkInfo(std::shared_ptr<SIPMessageInfo> info, quint32 connectionID);
  void newSIPLinkFromMessage(std::shared_ptr<SIPMessageInfo> info, quint32 connectionID);

  QString generateRandomString(uint32_t length);

  QList<QHostAddress> parseIPAddress(QString address);

  // helper function that composes SIP message and sends it
  void sendRequest(MessageType request, std::shared_ptr<SIPLink> link);



  std::map<QString, std::shared_ptr<SIPLink>> sessions_;

  QMutex connectionMutex_;
  std::vector<Connection*> connections_;

  SIPStringComposer messageComposer_;

  uint16_t sipPort_;

  QString ourName_;
  QString ourUsername_;
  //QHostAddress ourLocation_;

  // listens to incoming tcp connections on sipPort_
  ConnectionServer server_;
};
