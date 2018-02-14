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

  void initConnection(ConnectionType type, QHostAddress target);
  void receiveTCPConnection(std::shared_ptr<TCPConnection> con);

  void sendRequest(SIPRequest request);
  void sendResponse(SIPResponse response);

public slots:

  void networkPackage(QString message);

signals:

  void incomingSIPRequest(SIPRequest request,
                          quint32 sessionID_);

  void incomingSIPResponse(SIPResponse response,
                           quint32 sessionID_);

  void parsingError(ResponseType errorResponse, quint32 sessionID);

private:

  void parsePackage(QString package, QString& header, QString& body);

  void parseSIPaddress(QString address, QString& user, QString& location);

  QList<QHostAddress> parseIPAddress(QString address);

  bool parseSIPHeader(QString header);

  QString partialMessage_;

  TCPConnection connection_;

  quint32 sessionID_;
};
