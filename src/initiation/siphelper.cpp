#include "siphelper.h"

#include <QStringList>

/* https://en.wikipedia.org/wiki/Private_network */
bool isPrivateNetwork(const std::string &address)
{
  if (address.rfind("10.", 0) == 0 || address.rfind("192.168", 0) == 0 || address.rfind("fd", 0) == 0)
  {
    return true;
  }

  // link-local
  if (address.rfind("169.254.", 0) == 0 || address.rfind("fe80::", 0) == 0)
  {
    return true;
  }

  if (address.rfind("172.", 0) == 0)
  {
    if (address.size() >= 10 && address[6] == '.')
    {
      // checks that second octet is between 16 and 31
      int octet = std::stoi(address.substr(4, 2));
      return octet >= 16 && octet <= 31;
    }
  }

  return false;
}
