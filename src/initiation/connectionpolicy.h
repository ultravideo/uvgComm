#pragma once

#include <string>
#include <vector>

/* This class is responsible for determining if the incoming connection
 * is allowed */

// TODO: Implement this class

class ConnectionPolicy
{
public:
  ConnectionPolicy();

  bool isAllowed(std::string localUsername, std::string theirUsername, std::string serverAddress,
                 std::string contactAddress);

private:
  void initializePolicy();

  std::vector<std::string> blockedUsers_;
  std::vector<std::string> blockedIPs_;
};

