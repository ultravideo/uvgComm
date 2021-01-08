#pragma once

#include "initiation/siptypes.h"
#include "initiation/negotiation/sdptypes.h"
#include "tcpconnection.h"
#include "siprouting.h"
#include <QHostAddress>
#include <QString>

#include <memory>

// SIP Transportation layer. Use separate connection class to actually send the messages.
// This class primarily deals with checking that the incoming messages are valid, parsing them
// and composing outgoing messages.

class StatisticsInterface;

class SIPTransport : public QObject
{
  Q_OBJECT
public:
  SIPTransport(quint32 transportID, StatisticsInterface *stats);
  ~SIPTransport();

  void cleanup();

  // TODO: separate non-dialog and dialog messages

  // functions for manipulating network connection
  void createConnection(SIPTransportProtocol type, QString target);
  void incomingTCPConnection(std::shared_ptr<TCPConnection> con);

  // sending SIP messages
  void sendRequest(SIPRequest &request, QVariant& content);
  void sendResponse(SIPResponse &response, QVariant& content);

  bool isConnected();

  QString getLocalAddress();
  QString getRemoteAddress();

  uint16_t getLocalPort();

  quint32 getTransportID()
  {
    return transportID_;
  }

public slots:
  // called when connection receives a message
  void networkPackage(QString package);

  // called when a connection is established and ip known.
  // This function may be replaced by something in the future
  void connectionEstablished(QString localAddress, QString remoteAddress);

signals:
  // signal that ads transportID to connectionEstablished slot
  void sipTransportEstablished(quint32 transportID, QString localAddress,
                               QString remoteAddress);

  // signals that output parsed sip messages
  void incomingSIPRequest(SIPRequest& request, QString localAddress,
                          QVariant& content, quint32 transportID);
  void incomingSIPResponse(SIPResponse& response, QVariant& content);

  // we got a message, but could not parse it.
  void parsingError(SIPResponseStatus errorResponse, quint32 transportID);

private:

  // returs true if the whole message was received
  bool parsePackage(QString package, QStringList &headers, QStringList &bodies);

  bool parseRequest(QString requestString, QString version,
                    QList<SIPField>& fields, QString &body);
  bool parseResponse(QString responseString, QString version, QString text,
                     QList<SIPField> &fields, QString &body);

  void signalConnections();
  void destroyConnection();


  QString partialMessage_;

  std::shared_ptr<TCPConnection> connection_;
  quint32 transportID_;

  StatisticsInterface *stats_;

  SIPRouting routing_;

  int processingInProgress_;
};
