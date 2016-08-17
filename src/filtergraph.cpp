#include "filtergraph.h"


#include "camerafilter.h"
#include "displayfilter.h"
#include "kvazaarfilter.h"
#include "rgb32toyuv.h"
#include "openhevcfilter.h"
#include "yuvtorgb32.h"
#include "framedsourcefilter.h"
#include "displayfilter.h"


FilterGraph::FilterGraph():filters_()//, streamControl_()
{

}

void FilterGraph::init(in_addr ip, uint16_t port, VideoWidget* selfView, VideoWidget *peerView)
{
  streamer_.start();

  streamer_.addPeer(ip, true, true);


  initSender(selfView);


  Filter *framedSource = NULL;
  while(framedSource == NULL)
  {
    framedSource = streamer_.getSourceFilter(1);
  }
  filters_.push_back(framedSource);
  filters_.at(filters_.size() - 3)->addOutConnection(filters_.back());

  // Receiving video graph
  Filter* rtpSink = NULL;
  while(rtpSink == NULL)
  {
    rtpSink = streamer_.getSinkFilter(1);
  }
  filters_.push_back(rtpSink);

  OpenHEVCFilter* decoder =  new OpenHEVCFilter();
  decoder->init();
  filters_.push_back(decoder);
  filters_.at(filters_.size() - 2)->addOutConnection(filters_.back());

  filters_.push_back(new YUVtoRGB32());
  filters_.at(filters_.size() - 2)->addOutConnection(filters_.back());

  filters_.push_back(new DisplayFilter(peerView));
  filters_.at(filters_.size() - 2)->addOutConnection(filters_.back());
}


void FilterGraph::initSender(VideoWidget *selfView)
{
  // Sending video graph
  filters_.push_back(new CameraFilter());

  filters_.push_back(new RGB32toYUV());
  filters_.at(filters_.size() - 2)->addOutConnection(filters_.back());

  KvazaarFilter* kvz = new KvazaarFilter();
  kvz->init(640, 480, 15,1, 0);
  filters_.push_back(kvz);
  filters_.at(filters_.size() - 2)->addOutConnection(filters_.back());

  // connect selfview to camera
  DisplayFilter* selfviewFilter = new DisplayFilter(selfView);
  selfviewFilter->setProperties(true, QSize(128,96));
  filters_.push_back(selfviewFilter);
  filters_.at(0)->addOutConnection(filters_.back());
}

void FilterGraph::uninit()
{
  deconstruct();
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

  streamer_.start();
}

void FilterGraph::stop()
{
  for(Filter* f : filters_)
  {
    f->stop();
    f->emptyBuffer();
  }
  streamer_.stop();
}
