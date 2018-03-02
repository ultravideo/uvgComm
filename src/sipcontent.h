#pragma once

#include <QString>
#include <memory>

struct SDPMessageInfo;

QString SDPtoString(const std::shared_ptr<SDPMessageInfo> sdpInfo);

std::shared_ptr<SDPMessageInfo> parseSDPMessage(QString& body);


