#include "filtergraph.h"


#include "camerafilter.h"
#include "displayfilter.h"
#include "kvazaarfilter.h"
#include "rgb32toyuv.h"

FilterGraph::FilterGraph()
{

}

void FilterGraph::constructVideoGraph(VideoWidget *videoWidget)
{
  filters_.push_back(new CameraFilter);

  filters_.push_back(new RGB32toYUV);
  filters_.at(0)->addOutConnection(filters_.at(1));

  KvazaarFilter* kvz = new KvazaarFilter();
  kvz->init(640, 480, 15,1, 0);
  filters_.push_back(kvz);
  filters_.at(1)->addOutConnection(filters_.at(2));

  filters_.push_back(new DisplayFilter(videoWidget));
  filters_.at(2)->addOutConnection(filters_.at(3));

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
