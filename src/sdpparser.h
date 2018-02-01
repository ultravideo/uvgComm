#pragma once

#include <QString>
#include <QStringList>

#include <memory>

struct SDPMessageInfo;


std::shared_ptr<SDPMessageInfo> parseSDPMessage(QString& body);

bool checkSDPLine(QStringList& line, uint8_t expectedLength, QString& firstValue);
