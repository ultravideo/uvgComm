#pragma once

#include "initiation/siptypes.h"
#include "initiation/negotiation/sdptypes.h"
#include "initiation/sipmessageprocessor.h"
#include "tcpconnection.h"
#include "siprouting.h"
#include <QHostAddress>
#include <QString>

#include <memory>

// SIP Transportation layer. Use separate connection class to actually send the
// messages. This class primarily deals with checking that the incoming messages
// are valid, parsing them and composing outgoing messages.

class StatisticsInterface;

class SIPTransport : public SIPMessageProcessor
{
  Q_OBJECT
public:
  SIPTransport(StatisticsInterface *stats);
  ~SIPTransport();

  void cleanup();


  // functions for manipulating network connection
  void createConnection(SIPTransportProtocol type, QString target);
  void incomingTCPConnection(std::shared_ptr<TCPConnection> con);

    // these translate the struct to a string and send the SIP message
  virtual void processOutgoingRequest(SIPRequest& request, QVariant& content);
  virtual void processOutgoingResponse(SIPResponse& response, QVariant& content);

  bool isConnected();

  QString getLocalAddress();
  QString getRemoteAddress();

  uint16_t getLocalPort();

public slots:
  // called when connection receives a message
  void networkPackage(QString package);

  // called when a connection is established and ip known.
  // This function may be replaced by something in the future
  void connectionEstablished(QString localAddress, QString remoteAddress);

signals:
  void sipTransportEstablished(QString localAddress, QString remoteAddress);

  // signals that output parsed sip messages
  void incomingRequest(SIPRequest& request, QVariant& content,
                       QString localAddress);
  void incomingResponse(SIPResponse& response, QVariant& content,
                        QString localAddress);

  // we got a message, but could not parse it.
  void parsingError(SIPResponseStatus errorResponse, QString source);

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

  StatisticsInterface *stats_;

  std::shared_ptr<SIPRouting> routing_;

  int processingInProgress_;
};
