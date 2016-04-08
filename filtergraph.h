#ifndef FILTERGRAPH_H
#define FILTERGRAPH_H


#include <vector>
#include "filter.h"

class FilterGraph
{
public:
    FilterGraph();

    void constructVideoGraph();
    void constructAudioGraph();

    void run();

    void stop();

    void deconstruct();


private:

    std::vector<Filter*> filters;
};

#endif // FILTERGRAPH_H
