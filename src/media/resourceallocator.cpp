#include "resourceallocator.h"

#include "processing/yuvconversions.h"

#include "settingskeys.h"
#include "logger.h"
#include "common.h"

ResourceAllocator::ResourceAllocator()
{
  sse41_ = is_sse41_available();
  avx2_  = is_avx2_available();
  manualROI_ = false;
}


void ResourceAllocator::updateSettings()
{
  Logger::getLogger()->printNormal(this, "Updating automatic resource controller settings");
  manualROI_ = settingEnabled(SettingsKey::manualROIStatus);
  roiQp_ = settingValue(SettingsKey::RoiQp);
  backgroundQp_ = settingValue(SettingsKey::backgroundQp);
}


bool ResourceAllocator::isAVX2Enabled()
{
  return avx2_;
}


bool ResourceAllocator::isSSE41Enabled()
{
  return sse41_;
}


bool ResourceAllocator::useManualROI()
{
  return manualROI_;
}

uint8_t ResourceAllocator::getRoiQp() const
{
  return roiQp_;
}

uint8_t ResourceAllocator::getBackgroundQp() const
{
  return backgroundQp_;
}
