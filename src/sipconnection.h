#pragma once


#include "siptypes.h"
#include <connection.h>

#include <QHostAddress>
#include <QString>

#include <memory>

enum ConnectionType {ANY, TCP, UDP, TSL};

struct SIPParameter
{
  QString name;
  QString value;
};

struct SIPField
{
  QString name;
  QList<QString>* values;
  QList<SIPParameter>* parameters;
};



class SIPConnection : public QObject
{
  Q_OBJECT
public:
  SIPConnection(quint32 sessionID);
  ~SIPConnection();

  void initConnection(ConnectionType type, QHostAddress target);

  void sendRequest(RequestType request,
                   std::shared_ptr<SIPRoutingInfo> routing,
                   std::shared_ptr<SIPSessionInfo> session,
                   std::shared_ptr<SIPMessageInfo> message);

  void sendResponse(ResponseType response,
                    std::shared_ptr<SIPRoutingInfo> routing,
                    std::shared_ptr<SIPSessionInfo> session,
                    std::shared_ptr<SIPMessageInfo> message);
public slots:

  void networkPackage(QString message);

signals:

  void incomingSIPRequest(RequestType request,
                          std::shared_ptr<SIPRoutingInfo> routing,
                          std::shared_ptr<SIPSessionInfo> session,
                          std::shared_ptr<SIPMessageInfo> message,
                          quint32 sessionID_);

  void incomingSIPResponse(ResponseType response,
                           std::shared_ptr<SIPRoutingInfo> routing,
                           std::shared_ptr<SIPSessionInfo> session,
                           std::shared_ptr<SIPMessageInfo> message,
                           quint32 sessionID_);

  void incomingMalformedRequest(quint32 sessionID_);

private:

  void parsePackage(QString package, QString& header, QString& body);
  std::shared_ptr<QList<SIPField>> networkToFields(QString header,
                                                   std::shared_ptr<QStringList> firstLine);
  bool checkFields(std::shared_ptr<QStringList> firstLine,
                   std::shared_ptr<QList<SIPField>> fields);
  void processFields(std::shared_ptr<QList<SIPField>> fields);

  std::shared_ptr<SDPMessageInfo> parseSDPMessage(QString& body);

  void parseSIPaddress(QString address, QString& user, QString& location);
  void parseSIPParameter(QString field, QString parameterName,
                         QString& parameterValue, QString& remaining);
  QList<QHostAddress> parseIPAddress(QString address);

  bool checkSDPLine(QStringList& line, uint8_t expectedLength, QString& firstValue);

  QString partialMessage_;

  Connection connection_;

  quint32 sessionID_;
};

std::shared_ptr<SDPMessageInfo> parseSDPMessage(QString& body);

QList<QHostAddress> parseIPAddress(QString address);





