#pragma once

#include "initiation/transport/connectionserver.h"
#include "initiation/transaction/siptransactions.h"

class SIPTransactionUser;


class SIPManager : public QObject
{
  Q_OBJECT
public:
  SIPManager();

  // start listening to incoming
  void init(SIPTransactionUser* callControl);
  void uninit();

  // REGISTER our information to SIP-registrar
  void bindToServer();
  // start a call with address. Returns generated sessionID
  uint32_t startCall(Contact& address);

  // transaction user wants something.
  void acceptCall(uint32_t sessionID);
  void rejectCall(uint32_t sessionID);
  void cancelCall(uint32_t sessionID);

  void endCall(uint32_t sessionID);
  void endAllCalls();

  void getSDPs(uint32_t sessionID,
               std::shared_ptr<SDPMessageInfo>& localSDP,
               std::shared_ptr<SDPMessageInfo>& remoteSDP);

private slots:

  void receiveTCPConnection(TCPConnection* con);
  void connectionEstablished(quint32 transportID);

  void transportRequest(uint32_t sessionID, SIPRequest &request, QVariant& content);
  void transportResponse(uint32_t sessionID, SIPResponse &response, QVariant& content);

  void processSIPRequest(SIPRequest &request, QHostAddress localAddress,
                         QVariant& content, quint32 transportID);

private:

  std::shared_ptr<SIPTransport> createSIPTransport();

  bool isConnected(QString remoteAddress, quint32& transportID);

  ConnectionServer tcpServer_;
  uint16_t sipPort_;
  QMap<quint32, std::shared_ptr<SIPTransport>> transports_;
  quint32 nextTransportID_;

  SIPTransactions transactions_;

  std::map<uint32_t, quint32> sessionToTransportID_;

  struct WaitingStart
  {
    uint32_t sessionID;
    Contact contact;
  };

  std::map<quint32, WaitingStart> waitingToStart_;
  std::map<quint32, QString> waitingToBind_;
};
