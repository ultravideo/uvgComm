#include "scripting.h"

#include "logger.h"

Scripting::Scripting()
{}


void Scripting::fileScripting(const QString& filename)
{
  Logger::getLogger()->printNormal(this, "Running script from file: " + filename);
}


void Scripting::stdinScripting()
{
  Logger::getLogger()->printNormal(this, "Running script from stdin");
}


void Scripting::start()
{
  Logger::getLogger()->printNormal(this, "Starting script execution");
}
