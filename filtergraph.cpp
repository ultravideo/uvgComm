#include "filtergraph.h"


#include "camerafilter.h"

FilterGraph::FilterGraph()
{

}

void FilterGraph::constructVideoGraph()
{
    filters_.push_back(new CameraFilter);
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
        f->stop();
    }
}
