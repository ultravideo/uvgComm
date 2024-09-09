#include "ssrcgenerator.h"

#include "logger.h"

#include <chrono>


std::unique_ptr<SSRCGenerator> SSRCGenerator::instance_ = nullptr;

SSRCGenerator::SSRCGenerator():
  rng_()
{
  unsigned int now = 0;
  now = static_cast<unsigned int>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
  rng_.seed(std::random_device{}() + now);
}


SSRCGenerator::~SSRCGenerator()
{}


uint32_t SSRCGenerator::generateSSRC()
{
  if (SSRCGenerator::instance_ == nullptr)
  {
    SSRCGenerator::instance_ = std::unique_ptr<SSRCGenerator>(new SSRCGenerator());
  }

  return instance_->newSSRC();
}


uint32_t SSRCGenerator::newSSRC()
{
  std::uniform_int_distribution<uint32_t> gen32_dist{1, UINT32_MAX};

  uint32_t ssrc = gen32_dist(rng_);

  while (previousSSRCs_.find(ssrc) != previousSSRCs_.end())
  {
    Logger::getLogger()->printDebug(DEBUG_WARNING, "SSRCGenerator",
                                    "SSRC collision while generating");
    ssrc = gen32_dist(rng_);
  }

  previousSSRCs_.insert(ssrc);

  return ssrc;
}
