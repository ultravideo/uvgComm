#pragma once

#include <QStringList>


class DeviceInfoInterface
{
public:
 virtual ~DeviceInfoInterface(){}

 // returns the list of available devices in string list.
 virtual QStringList getDeviceList() = 0;
};
