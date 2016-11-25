#pragma once


#ifdef Q_OS_WIN
  #include <winsock2.h>
  #include <osip2/osip.h>
#else
  #include <sys/time.h>
  #include <osip2/osip.h>
#endif

#include <MediaSession.hh>

class CallNegotiation
{
public:
  CallNegotiation();
  ~CallNegotiation();

  void init();

  void setupSession(MediaSubsession* subsession);

  void sendINVITE();

  // accepts INVITE command
  void sendACK();

private:

  osip_t* osip_;
};
