#include "filtergraph.h"


#include "camerafilter.h"
#include "displayfilter.h"


FilterGraph::FilterGraph()
{

}

void FilterGraph::constructVideoGraph(VideoWidget *videoWidget)
{
  filters_.push_back(new CameraFilter);
  filters_.push_back(new DisplayFilter(videoWidget));
  filters_.at(0)->addOutConnection(filters_.at(1));
}

void FilterGraph::constructAudioGraph()
{}

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
    f->stop();
    f->emptyBuffer();
  }
}
