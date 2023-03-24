#pragma once

#include "icetypes.h"

#include <QString>

#include <stdint.h>

#include <memory>
#include <vector>

struct MediaInfo;

// A module for functions used multiple times across the whole program.
// Try to keep printing of same information consequatively to a minimum and
// instead try to combine the string to more information rich statement.

// generates a random string of length.
QString generateRandomString(uint32_t length);

bool settingEnabled(QString key);
int settingValue(QString key);

QString settingString(QString key);

QString getLocalUsername();

bool isLocalCandidate(std::shared_ptr<ICEInfo> info);

bool getSendAttribute(const MediaInfo &media, bool local);
bool getReceiveAttribute(const MediaInfo &media, bool local);

void setSDPAddress(QString inAddress, QString& address,
                   QString& nettype, QString& addressType);
bool areMediasEqual(const MediaInfo first, const MediaInfo second);

bool sameCandidates(std::vector<std::shared_ptr<ICEPair>> newCandidates,
                    std::vector<std::shared_ptr<ICEPair>> oldCandidates);

bool containCandidates(std::vector<std::shared_ptr<ICEPair> > &streams,
                       std::vector<std::shared_ptr<ICEPair>> allCandidates);

bool sameCandidate(std::shared_ptr<ICEInfo> firstCandidate,
                   std::shared_ptr<ICEInfo> secondCandidate);
