#include "callnegotiation.h"

CallNegotiation::CallNegotiation()
  :
    osip_(NULL)
{}

CallNegotiation::~CallNegotiation()
{
  if(osip_)
  {
    delete osip_;
  }
}

void CallNegotiation::init()
{

}
