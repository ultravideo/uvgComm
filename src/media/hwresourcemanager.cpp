#include "hwresourcemanager.h"

#include "processing/yuvconversions.h"


HWResourceManager::HWResourceManager()
{
  sse41_ = is_sse41_available();
  avx2_  = is_avx2_available();
}


bool HWResourceManager::isAVX2Enabled()
{
  return avx2_;
}


bool HWResourceManager::isSSE41Enabled()
{
  return sse41_;
}
