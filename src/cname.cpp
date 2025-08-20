#include "cname.h"

#include "settingskeys.h"

#include "common.h"

#include <QSettings>

#include <chrono>

std::shared_ptr<CName> CName::instance_ = nullptr;

CName::CName():
    cname_("")
{}


QString CName::cname()
{
  if (instance_ == nullptr)
  {
    instance_ = std::shared_ptr<CName>(new CName());
  }

  return instance_->getCName();
}


void CName::generateCName()
{
  QSettings settings = QSettings(getSettingsFile(), settingsFileFormat);

  cname_ = settings.value(SettingsKey::sipUUID).toString() + "@"
           + QString::number(std::chrono::system_clock::now().time_since_epoch().count());
}


QString CName::getCName() const
{
  if (instance_->cname_ == "")
  {
    instance_->generateCName();
  }

  return cname_;
}
