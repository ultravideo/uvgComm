#include "sdpparametermanager.h"

SDPParameterManager::SDPParameterManager()
{}

QStringList SDPParameterManager::audioCodecs() const
{
  return QStringList{"opus"};
}

QStringList SDPParameterManager::videoCodecs() const
{
  return QStringList{"hevc"};
}

QString SDPParameterManager::sessionName() const
{
  return " ";
}

QString SDPParameterManager::sessionDescription() const
{
  return "A Kvazzup Video Conference";
}
