#pragma once

#include "filter.h"

class LibYUVConverter : public Filter
{
public:
    LibYUVConverter(QString id, StatisticsInterface* stats,
                    std::shared_ptr<ResourceAllocator> hwResources, DataType input);

    virtual void updateSettings();

protected:

    void process();

};
