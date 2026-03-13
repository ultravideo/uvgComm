#pragma once

#include "icetypes.h"
#include "qhostaddress.h"

#include <QString>
#include <QSize>

#include <stdint.h>

#include <chrono>

#include <memory>
#include <vector>

struct MediaInfo;

// Returns a monotonically increasing timestamp in milliseconds.
// Used for latency measurements; implementation can be swapped centrally.
inline int64_t clockNowMs()
{
    const auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

// A module for functions used multiple times across the whole program.
// Try to keep printing of same information consequatively to a minimum and
// instead try to combine the string to more information rich statement.

// generates a random string of length.
QString generateRandomString(uint32_t length);

QString getSettingsFile();
void setSettingsFile(QString filename);

bool settingEnabled(QString key);
int settingValue(QString key);

QString settingString(QString key);
bool settingBool(QString key);

QString getLocalUsername();

bool isLocalCandidate(std::shared_ptr<ICEInfo> info);
bool isLocalAddress(QString candidateAddress);

bool getSendAttribute(const MediaInfo &media, bool local);
bool getReceiveAttribute(const MediaInfo &media, bool local);

void getMediaAttributes(const MediaInfo &local, const MediaInfo& remote,
                        bool followOurSDP, bool &send,
                        bool& receive);

void setSDPAddress(QString inAddress, QString& address,
                   QString& nettype, QString& addressType);

bool sameCandidates(std::vector<std::shared_ptr<ICEPair>> newCandidates,
                    std::vector<std::shared_ptr<ICEPair>> oldCandidates);

bool containCandidates(std::vector<std::shared_ptr<ICEPair> > &streams,
                       std::vector<std::shared_ptr<ICEPair>> allCandidates);

bool sameCandidate(std::shared_ptr<ICEInfo> firstCandidate,
                   std::shared_ptr<ICEInfo> secondCandidate);

void printIceCandidates(QString text, QList<std::shared_ptr<ICEInfo>> candidates);

uint64_t msecToNTP(int64_t msec);
int64_t NTPToMsec(uint64_t ntp);

QSize galleryResolution(QSize baseResolution, uint32_t otherParticipants);
QSize speakerResolution(QSize baseResolution, uint32_t otherParticipants);
QSize listenerResolution(QSize baseResolution, uint32_t otherParticipants);

int32_t galleryBitrate(QSize baseResolution, int baseBitrate, uint32_t otherParticipants);
int32_t speakerBitrate(QSize baseResolution, int baseBitrate, uint32_t otherParticipants);
int32_t listenerBitrate(QSize baseResolution, int baseBitrate, uint32_t otherParticipants);

// helper functions that get either actual address/port or
// relay address/port if needed
QHostAddress getLocalAddress(std::shared_ptr<ICEInfo> info);
quint16 getLocalPort(std::shared_ptr<ICEInfo> info);

uint32_t findSSRC(const MediaInfo &media);

bool findSSRCs(const MediaInfo& media, std::vector<uint32_t>& ssrc);
bool findCNAMEs(const MediaInfo &media, std::vector<QString> &cnames);

QString findMID(const MediaInfo &media);
