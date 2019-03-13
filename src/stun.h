#pragma once

#include "udpserver.h"

#include <QHostInfo>
#include <QHostAddress>
#include <QDnsLookup>
#include <QMap>

#if 0
enum {
  hello = 1,
} STUN_ATTR;
#endif

enum STUN_TYPES {
  STUN_REQUEST  = 0x0001,
  STUN_RESPONSE = 0x0101
};

enum STUN_ATTRIBUTES {
  PRIORITY = 0x0024,
  USE_CANDIATE = 0x0025,
  ICE_CONTROLLING = 0x802A,
  ICE_CONTROLLED = 0x8029,
};

struct STUNMessage
{
  uint16_t type;
  uint16_t length;
  uint32_t magicCookie;
  uint8_t transactionID[12];
  unsigned char payload[0];
};

class Stun : public QObject
{
  Q_OBJECT
public:
  Stun();

  int sendRequest(STUNMessage *message);

  // send stun binding request to remote 
  // this function is used to establish a gateway between clients
  //
  // return true if we got a response from remote and false if
  // after N tries the remote was silent
  bool sendBindingRequest(QString addressRemote, int portRemote, QString addressLocal, int portLocal, bool controller);

  // TODO add comment
  bool sendBindingResponse(QString addressRemote, int portRemote);

  // TODO add comment
  //
  // controller calls this function 
  //
  // return true if the nomination was successful
  // and false if we didn't get a response

  // Send the nominated candidate to ICE_CONTROLLED agent 
  bool sendNominationRequest(QString addressRemote, int portRemote, QString addressLocal, int portLocal);

  // TODO add comment
  //
  // controllee calls this function
  //
  // return true if the nomination was successful
  // and false if we didn't get a response


  // this function is called by the ICE_CONTROLLED agent
  //
  // It is called when the two clients have negotiated a list of possible candidates.
  // The ICE_CONTROLLING agent selects RTP and RTCP candidates used for media streaming
  // and sends these two candidates using STUN Binding request
  //
  // ICE_CONTROLLED agent (caller of sendNominationResponse()) must send STUN Binding Requests
  // to all valid candidate address:port pairs to receive the nominations
  //
  //
  bool sendNominationResponse(QString addressRemote, int portRemote, QString addressLocal, int portLocal);

  void wantAddress(QString stunServer);

signals:
  void addressReceived(QHostAddress address);
  void stunError();
  void parsingDone();

private slots:
  void handleHostaddress(QHostInfo info);
  void processReply(QByteArray data);
  void recvStunMessage(QNetworkDatagram message);

private:
  // generate genereic stun message and
  // save the transaction id to transactionID_ for reply identification
  QByteArray generateMessage();
  STUNMessage getStunMessage(QByteArray data);

  STUNMessage *generateResponse(size_t payloadSize, void *payload);

  bool validateStunResponse(QByteArray data);
  bool validateStunRequest(QByteArray data);
  bool validateStunMessage(QByteArray data, int type);

  // TODO 
  bool waitForStunResponse(unsigned long timeout);

  // TODO [Encryption] Use TLS to send packet
  UDPServer udp_;

  uint8_t transactionID_[12];
};
