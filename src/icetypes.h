#pragma once

enum PAIR {
  PAIR_WAITING     = 0,
  PAIR_IN_PROGRESS = 1,
  PAIR_SUCCEEDED   = 2,
  PAIR_FAILED      = 3,
  PAIR_FROZEN      = 4,
};

enum COMPONENTS {
  RTP  = 1,
  RTCP = 2
};

/* list of ICEInfo (candidates) is send during INVITE */
struct ICEInfo
{
  QString foundation;  /* TODO:  */
  int component;       /* 1 for RTP, 2 for RTCP */
  QString transport;   /* UDP/TCP */
  int priority;        /* TODO: */

  QString address;
  int port;

  QString type;        /* host/relayed */
  QString rel_address; /* for turn, not used (currently)  */
};

struct ICEPair
{
  struct ICEInfo *local;
  struct ICEInfo *remote;
  int priority;
  int state;
};

