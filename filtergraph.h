#pragma once

#include <vector>
#include "filter.h"





class FilterGraph
{
public:
    FilterGraph();

    void constructVideoGraph();
    void constructAudioGraph();
    void deconstruct();
    void run();
    void stop();


private:

    // process
    void filterLoop();

    std::vector<Filter*> filters;
    std::vector<std::vector<unsigned int> > connections_;

};
