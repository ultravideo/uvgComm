#pragma once

#include "initiation/sipmessageprocessor.h"

#include "initiation/siptypes.h"

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

    // these translate the struct to a string and send the SIP message
  virtual void processOutgoingRequest(SIPRequest& request, QVariant& content);
  virtual void processOutgoingResponse(SIPResponse& response, QVariant& content);

public slots:
  // called when connection receives a message
  void networkPackage(QString package);

signals:
  void sipTransportEstablished(QString localAddress, QString remoteAddress);

  // we got a message, but could not parse it.
  void parsingError(SIPResponseStatus errorResponse, QString source);

  void sendMessage(const QString& message);

private:

  // returs true if the whole message was received
  bool parsePackage(QString package, QStringList &headers, QStringList &bodies);

  bool parseRequest(const QString& header, QString requestString, QString version,
                    QList<SIPField>& fields, QString &body);
  bool parseResponse(const QString& header, QString responseString, QString version, QString text,
                     QList<SIPField> &fields, QString &body);

  void signalConnections();


  QString partialMessage_;

  StatisticsInterface *stats_;

  int processingInProgress_;
};
