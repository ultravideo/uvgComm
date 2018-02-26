#pragma once

#include "siptypes.h"
#include "tcpconnection.h"
#include "sipstringcomposer.h"

#include <QHostAddress>
#include <QString>

#include <memory>

class SIPConnection : public QObject
{
  Q_OBJECT
public:
  SIPConnection(quint32 sessionID);
  ~SIPConnection();

  void createConnection(ConnectionType type, QString target);
  void incomingTCPConnection(std::shared_ptr<TCPConnection> con);
  void destroyConnection();

  void sendRequest(SIPRequest &request, std::shared_ptr<SDPMessageInfo> sdp);
  void sendResponse(SIPResponse &response, std::shared_ptr<SDPMessageInfo> sdp);

public slots:

  void networkPackage(QString message);
  void connectionEstablished(QString localAddress, QString remoteAddress);

signals:

  void incomingSIPRequest(SIPRequest request, quint32 sessionID);
  void incomingSIPResponse(SIPResponse response, quint32 sessionID);

  void parsingError(ResponseType errorResponse, quint32 sessionID);

  void sipConnectionEstablished(quint32 sessionID, QString localAddress, QString remoteAddress);

private:

  void composeHelper(uint32_t id, std::shared_ptr<SIPMessageInfo> message);

  void parsePackage(QString package, QString& header, QString& body);
  bool parseSIPHeader(QString header);
  void parseSIPaddress(QString address, QString& user, QString& location);
  QList<QHostAddress> parseIPAddress(QString address);

  void signalConnections();

  QString partialMessage_;
  SIPStringComposer messageComposer_;

  std::shared_ptr<TCPConnection> connection_;
  quint32 sessionID_;
};
