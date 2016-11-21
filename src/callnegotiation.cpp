#include "callnegotiation.h"

CallNegotiation::CallNegotiation()
  :
    client_(NULL)
{}

CallNegotiation::~CallNegotiation()
{

}

void CallNegotiation::createClient(UsageEnvironment &env, int verbosityLevel, const char *applicationName)
{
  unsigned char desiredAudioRTPPayloadFormat = 96;
  char* mimeSubtype = "opus";
  client_ = SIPClient::createNew(env, desiredAudioRTPPayloadFormat, mimeSubtype, verbosityLevel, applicationName);
}
