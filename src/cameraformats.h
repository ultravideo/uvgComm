#pragma once

#include "media/processing/filter.h"

#include <QVideoFrameFormat>
#include <QString>

QString videoFormatToString(QVideoFrameFormat::PixelFormat format);

DataType videoFormatToDataType(QVideoFrameFormat::PixelFormat format);

QVideoFrameFormat::PixelFormat stringToPixelFormat(QString format);

DataType stringToDataType(QString format);

QString resolutionToString(QSize resolution);

