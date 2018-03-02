#pragma once

#include <QString>

#include <memory>

class SDPMessageInfo;

QString composeSDPContent(const std::shared_ptr<SDPMessageInfo> sdp);
std::shared_ptr<SDPMessageInfo> parseSDPContent(const QString& content);

