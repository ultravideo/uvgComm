#pragma once

#include "udpserver.h"
#include "stunmsg.h"

#include <QHostInfo>
#include <QHostAddress>
#include <QDnsLookup>
#include <QMap>

class Stun : public QObject
{
  Q_OBJECT
public:
  Stun();

  // send stun binding request to remote 
  // this function is used to establish a gateway between clients
  //
  // return true if we got a response from remote and false if
  // after N tries the remote was silent
  bool sendBindingRequest(QString addressRemote, int portRemote, QString addressLocal, int portLocal, bool controller);

  // TODO add comment
  bool sendBindingResponse(QString addressRemote, int portRemote);

  // Send the nominated candidate to ICE_CONTROLLED agent 
  bool sendNominationRequest(QString addressRemote, int portRemote, QString addressLocal, int portLocal);

  // this function is called by the ICE_CONTROLLED agent
  //
  // It is called when the two clients have negotiated a list of possible candidates.
  // The ICE_CONTROLLING agent selects RTP and RTCP candidates used for media streaming
  // and sends these two candidates using STUN Binding request
  //
  // ICE_CONTROLLED agent (caller of sendNominationResponse()) must send STUN Binding Requests
  // to all valid candidate address:port pairs to receive the nominations
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
  bool waitForStunResponse(unsigned long timeout);

  // TODO [Encryption] Use TLS to send packet
  UDPServer udp_;

  StunMessageFactory stunmsg_;

  uint8_t transactionID_[12];
};
