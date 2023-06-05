#pragma once

#include "filter.h"

class DetectionFilter : public Filter {
public:
    DetectionFilter(QString id, StatisticsInterface* stats, std::shared_ptr<ResourceAllocator> hwResources)
        : Filter(id, "DetectionFilter", stats,
                 hwResources,
                 DataType::DT_YUV420VIDEO, DataType::DT_RGB32VIDEO, false)
    {

    }
    ~DetectionFilter() {}

    void process()
    {
        std::unique_ptr<Data> input = getInput();
        while (input)
        {
            input->type = DT_DETECTIONS;
            sendOutput(std::move(input));
            input = getInput();
        }
    }

};
