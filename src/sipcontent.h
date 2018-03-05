#pragma once

#include <QString>

#include <memory>

class SDPMessageInfo;

QString composeSDPContent(const SDPMessageInfo& sdp);
std::shared_ptr<SDPMessageInfo> parseSDPContent(const QString& content);

