#include "filtergraph.h"

#include "logger.h"
#include "Filter.h"

#include "media/processing/libyuvconverter.h"
#include "media/processing/yuvtorgb32.h"

FilterGraph::FilterGraph():
  quitting_(false),
  stats_(nullptr),
  hwResources_(nullptr)
{}


void FilterGraph::init(StatisticsInterface* stats, std::shared_ptr<ResourceAllocator> hwResources)
{
  Q_ASSERT(stats);

  uninit();

  quitting_ = false;
  stats_ = stats;
  hwResources_ = hwResources;
}


bool FilterGraph::addToGraph(std::shared_ptr<Filter> filter,
                                GraphSegment &graph,
                                size_t connectIndex)
{
  // // check if we need some sort of conversion and connect to index
  if(graph.size() > 0 && connectIndex <= graph.size() - 1)
  {
    if(graph.at(connectIndex)->outputType() != filter->inputType())
    {
      Logger::getLogger()->printDebug(DEBUG_NORMAL, this,
                                      "Filter output and input do not match. Finding conversion",
                                      {"Connection"}, {graph.at(connectIndex)->getName() + "->" + filter->getName()});

      if (graph.at(connectIndex)->outputType() == DT_NONE)
      {
        Logger::getLogger()->printProgramError(this, "The previous filter has no output!");
        return false;
      }

      // TODO: Check the out connections of connected filter for an already existing conversion.

      if(filter->inputType() == DT_YUV420VIDEO)
      {
        Logger::getLogger()->printNormal(this, "Adding libyuv conversion filter to YUV420");
        addToGraph(std::shared_ptr<Filter>(new LibYUVConverter("", stats_, hwResources_,
                                                               graph.at(connectIndex)->outputType())),
                   graph, connectIndex);
      }
      else if(graph.at(connectIndex)->outputType() == DT_YUV420VIDEO &&
               filter->inputType() == DT_RGB32VIDEO)
      {
        Logger::getLogger()->printNormal(this, "Adding YUV420 to RGB32 conversion");
        addToGraph(std::shared_ptr<Filter>(new YUVtoRGB32("", stats_, hwResources_)),
                   graph, connectIndex);
      }
      else
      {
        Logger::getLogger()->printProgramError(this, "Could not find conversion for filter.");
        return false;
      }
      // the conversion filter has been added to the end
      connectIndex = (unsigned int)graph.size() - 1;
    }
    connectFilters(graph.at(connectIndex), filter);
  }

  graph.push_back(filter);
  if(filter->init())
  {
    filter->start();
  }
  else
  {
    Logger::getLogger()->printError(this, "Failed to init filter");
    return false;
  }
  return true;
}


bool FilterGraph::connectFilters(std::shared_ptr<Filter> previous, std::shared_ptr<Filter> filter)
{
  Q_ASSERT(filter != nullptr && previous != nullptr);

  Logger::getLogger()->printDebug(DEBUG_NORMAL, "FilterGraph", "Connecting filters",
                                  {"Connection"}, {previous->getName() + " -> " + filter->getName()});

  if(previous->outputType() != filter->inputType())
  {
    Logger::getLogger()->printDebug(DEBUG_WARNING, "FilterGraph",
                                    "The connecting filter output and input DO NOT MATCH.");
    return false;
  }
  previous->addOutConnection(filter);
  return true;
}
