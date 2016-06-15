#include "filtergraph.h"


#include "camerafilter.h"
#include "displayfilter.h"
#include "kvazaarfilter.h"
#include "rgb32toyuv.h"
#include "openhevcfilter.h"

FilterGraph::FilterGraph()
{

}

void FilterGraph::constructVideoGraph(VideoWidget *videoWidget)
{
  unsigned int currentFilter = 0;

  filters_.push_back(new CameraFilter);

  filters_.push_back(new RGB32toYUV);
  filters_.at(currentFilter)->addOutConnection(filters_.at(currentFilter + 1));
  currentFilter++;

  KvazaarFilter* kvz = new KvazaarFilter();
  kvz->init(640, 480, 15,1, 0);
  filters_.push_back(kvz);
  filters_.at(currentFilter)->addOutConnection(filters_.at(currentFilter + 1));
  currentFilter++;

  OpenHEVCFilter* decoder =  new OpenHEVCFilter();
  decoder->init();
  filters_.push_back(decoder);
  filters_.at(currentFilter)->addOutConnection(filters_.at(currentFilter + 1));
  currentFilter++;

  filters_.push_back(new DisplayFilter(videoWidget));
  filters_.at(currentFilter)->addOutConnection(filters_.at(currentFilter + 1));
  currentFilter++;

  Q_ASSERT(filters_[0]->isInputFilter());
  Q_ASSERT(!filters_[0]->isOutputFilter());
  Q_ASSERT(filters_[filters_.size() - 1]->isOutputFilter());
  Q_ASSERT(!filters_[filters_.size() - 1]->isInputFilter());
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
