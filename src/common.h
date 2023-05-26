#pragma once

#include "icetypes.h"
#include "qhostaddress.h"

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
bool isLocalAddress(QString candidateAddress);

bool getSendAttribute(const MediaInfo &media, bool local);
bool getReceiveAttribute(const MediaInfo &media, bool local);

void setSDPAddress(QString inAddress, QString& address,
                   QString& nettype, QString& addressType);

bool sameCandidates(std::vector<std::shared_ptr<ICEPair>> newCandidates,
                    std::vector<std::shared_ptr<ICEPair>> oldCandidates);

bool containCandidates(std::vector<std::shared_ptr<ICEPair> > &streams,
                       std::vector<std::shared_ptr<ICEPair>> allCandidates);

bool sameCandidate(std::shared_ptr<ICEInfo> firstCandidate,
                   std::shared_ptr<ICEInfo> secondCandidate);

void printIceCandidates(QString text, QList<std::shared_ptr<ICEInfo>> candidates);


// helper functions that get either actual address/port or
// relay address/port if needed
QHostAddress getLocalAddress(std::shared_ptr<ICEInfo> info);
quint16 getLocalPort(std::shared_ptr<ICEInfo> info);

uint32_t findSSRC(const MediaInfo &media);
uint32_t findMID(const MediaInfo &media);
