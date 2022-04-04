#include "resourceallocator.h"

#include "processing/yuvconversions.h"

#include "logger.h"

ResourceAllocator::ResourceAllocator()
{
  sse41_ = is_sse41_available();
  avx2_  = is_avx2_available();
}


bool ResourceAllocator::isAVX2Enabled()
{
  return avx2_;
}


bool ResourceAllocator::isSSE41Enabled()
{
  return sse41_;
}


void ResourceAllocator::updateSettings()
{
  Logger::getLogger()->printNormal(this, "Updating automatic resource controller settings");
}
