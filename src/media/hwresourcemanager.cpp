#include "hwresourcemanager.h"

// For additional optimizations checks:
// https://stackoverflow.com/questions/6121792/how-to-check-if-a-cpu-supports-the-sse3-instruction-set

#ifdef _WIN32
#include <intrin.h>
  #define cpuid(info, x)    __cpuidex(info, x, 0)
#else

//  GCC Intrinsics
#include <cpuid.h>
  void cpuid(int info[4], int InfoType){
      __cpuid_count(InfoType, 0, info[0], info[1], info[2], info[3]);
}

#endif


HWResourceManager::HWResourceManager()
{
  setOptimizations();
}


bool HWResourceManager::isAVX2Enabled()
{
  return avx2_;
}


bool HWResourceManager::isSSE41Enabled()
{
  return sse41_;
}


void HWResourceManager::setOptimizations()
{
  int info[4];
  cpuid(info, 0);
  int nIds = info[0];

  //  Detect Features
  if (nIds >= 0x00000001){
      cpuid(info,0x00000001);
      sse41_  = (info[2] & ((int)1 << 19)) != 0;
  }

  if (nIds >= 0x00000007){
      cpuid(info,0x00000007);
      avx2_   = (info[1] & ((int)1 <<  5)) != 0;
  }
}
