#pragma once

#include "siptypes.h"
#include "tcpconnection.h"
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

  void sendRequest(SIPRequest &request, QVariant content);
  void sendResponse(SIPResponse &response, QVariant content);

public slots:
  void networkPackage(QString message);
  void connectionEstablished(QString localAddress, QString remoteAddress);

signals:
  void sipConnectionEstablished(quint32 sessionID, QString localAddress, QString remoteAddress);

  void incomingSIPRequest(SIPRequest request, quint32 sessionID, QVariant content);
  void incomingSIPResponse(SIPResponse response, quint32 sessionID, QVariant content);

  void parsingError(ResponseType errorResponse, quint32 sessionID);

private:

  // parsing
  void parsePackage(QString package, QString& header, QString& body);
  bool parseSIPHeader(QString header);
  void parseSIPaddress(QString address, QString& user, QString& location);
  QList<QHostAddress> parseIPAddress(QString address);

  // composing
  bool composeMandatoryFields(QList<SIPField>& fields, std::shared_ptr<SIPMessageInfo> message);
  QString fieldsToString(QList<SIPField>& fields, QString lineEnding);
  QString addContent(QList<SIPField>& fields, bool haveContent, const SDPMessageInfo& sdp);

  void signalConnections();

  QString partialMessage_;

  std::shared_ptr<TCPConnection> connection_;
  quint32 sessionID_;
};
