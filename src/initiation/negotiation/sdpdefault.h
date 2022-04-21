#pragma once

#include "sdptypes.h"

#include <QString>
#include <QObject>

#include <memory>


std::shared_ptr<SDPMessageInfo> generateDefaultSDP(QString localAddress);




