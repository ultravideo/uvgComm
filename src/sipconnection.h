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

  void initConnection(ConnectionType type, QString target);
  void incomingTCPConnection(std::shared_ptr<TCPConnection> con);
  void destroyConnection();

  void sendRequest(SIPRequest &request, std::shared_ptr<SDPMessageInfo> sdp);
  void sendResponse(SIPResponse &response);

public slots:

  void networkPackage(QString message);
  void connectionEstablished();

signals:

  void incomingSIPRequest(SIPRequest request,
                          quint32 sessionID);

  void incomingSIPResponse(SIPResponse response,
                           quint32 sessionID);

  void parsingError(ResponseType errorResponse, quint32 sessionID);

  void sipConnectionEstablished(quint32 sessionID, QString localAddress, QString remoteAddress);

private:

  void parsePackage(QString package, QString& header, QString& body);

  void parseSIPaddress(QString address, QString& user, QString& location);

  QList<QHostAddress> parseIPAddress(QString address);

  bool parseSIPHeader(QString header);

  QString partialMessage_;
  SIPStringComposer messageComposer_;

  std::shared_ptr<TCPConnection> connection_;
  quint32 sessionID_;
};
