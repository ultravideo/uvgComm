#include "filtergraph.h"

FilterGraph::FilterGraph()
{

}

void FilterGraph::constructVideoGraph()
{

}

void FilterGraph::constructAudioGraph()
{

}

void FilterGraph::deconstruct()
{
    for( Filter *f : filters_ )
        delete f;

    filters_.clear();
}

void FilterGraph::run()
{
    for(Filter* f : filters_)
        f->start();
}

void FilterGraph::stop()
{
    for(Filter* f : filters_)
    {
        f->emptyBuffer();
        f->quit();
    }
}
