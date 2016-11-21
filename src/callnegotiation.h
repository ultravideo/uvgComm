#pragma once

#include <SIPClient.hh>


class CallNegotiation
{
public:
  CallNegotiation();
  ~CallNegotiation();

  void createClient(UsageEnvironment& env, int verbosityLevel, char const* applicationName);

  void setupSession(MediaSubsession* subsession);

private:


  SIPClient* client_;
};
