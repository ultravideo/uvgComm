#pragma once

#include "media/processing/filter.h"

#include <QVideoFrameFormat>
#include <QString>

QString videoFormatToString(QVideoFrameFormat::PixelFormat format);

QVideoFrameFormat::PixelFormat stringToPixelFormat(QString format);

QString resolutionToString(QSize resolution);

DataType stringToDatatype(QString format);
