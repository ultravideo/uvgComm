#pragma once

#include "sdptypes.h"

#include <QString>
#include <QObject>

#include <memory>


std::shared_ptr<SDPMessageInfo> generateLocalSDP(QString localAddress);

void negotiateSDP(std::shared_ptr<SDPMessageInfo> modifiedSDP,
                  SDPMessageInfo& remoteSDPOffer);

void generateOrigin(std::shared_ptr<SDPMessageInfo> sdp, QString localAddress);
void setConnectionAddress(std::shared_ptr<SDPMessageInfo> sdp, QString localAddress);

bool generateAudioMedia(MediaInfo &audio);
bool generateVideoMedia(MediaInfo &video);

bool selectBestCodec(QList<uint8_t>& remoteNums,       QList<RTPMap>& remoteCodecs,
                     QList<uint8_t>& supportedNums,    QList<RTPMap>& supportedCodecs,
                     QList<uint8_t>& outMatchingNums,  QList<RTPMap>& outMatchingCodecs);

// Checks if SDP is acceptable to us.
bool checkSDPOffer(SDPMessageInfo& offer);

// update MediaInfo of SDP after ICE has finished
void setMediaPair(MediaInfo& media, std::shared_ptr<ICEInfo> mediaInfo, bool local);
