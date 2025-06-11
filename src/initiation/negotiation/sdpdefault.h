#pragma once

#include "sdptypes.h"

#include <QString>
#include <QObject>

#include <memory>


// predefined codec type 0 (PCMU audio) is mandated to be always supported, otherwise
// it is recommended to specify everything using the dynamic types
std::shared_ptr<SDPMessageInfo> generateDefaultSDP(QString username,
                                                   QString localAddress,
                                                   int audioStreams = 0,
                                                   int videoStreams = 0,
                                                   QList<QString> dynamicAudioSubtypes = {},
                                                   QList<QString> dynamicVideoSubtypes = {},
                                                   QList<uint8_t> staticAudioPayloadTypes = {},
                                                   QList<uint8_t> staticVideoPayloadTypes = {},
                                                   uint16_t videoWidth = 0,
                                                   uint16_t videoHeight = 0,
                                                   uint32_t videoKbps = 0,
                                                   uint32_t audioKbps = 0);

void generateOrigin(std::shared_ptr<SDPMessageInfo> sdp, QString localAddress, QString username);

