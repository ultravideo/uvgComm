#pragma once

#include <QString>

#include <memory>

struct SDPMessageInfo;

// convert SDPMessageInfo to QString
QString composeSDPContent(const SDPMessageInfo& sdp);

// parse QString to SDPMessageInfo
bool parseSDPContent(const QString& content, SDPMessageInfo& sdp);

