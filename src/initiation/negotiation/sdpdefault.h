#pragma once

#include "sdptypes.h"

#include <QString>
#include <QObject>

#include <memory>


// predefined codec type 0 (PCMU audio) is mandated to be always supported, otherwise
// it is recommended to specify everything using the dynamic types
std::shared_ptr<SDPMessageInfo> generateDefaultSDP(QString username, QString localAddress,
                                                   int audioStreams = 0, int videoStreams = 0,
                                                   QList<QString> dynamicAudioSubtypes = {},
                                                   QList<QString> dynamicVideoSubtypes = {},
                                                   QList<uint8_t> staticAudioPayloadTypes = {},
                                                   QList<uint8_t> staticVideoPayloadTypes = {});

void setSDPAddress(QString inAddress, QString& sdpAddress,
                   QString& type, QString& addressType);

void generateOrigin(std::shared_ptr<SDPMessageInfo> sdp, QString localAddress, QString username);

