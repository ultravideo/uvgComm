#pragma once

#include <QString>

#include <memory>

enum PairState {
  PAIR_WAITING     = 0,
  PAIR_IN_PROGRESS = 1,
  PAIR_SUCCEEDED   = 2,
  PAIR_FAILED      = 3,
  PAIR_FROZEN      = 4,
  PAIR_NOMINATED   = 5,
};

enum CandidateType {
  RELAY = 0,
  SERVER_REFLEXIVE = 100,
  PEER_REFLEXIVE = 110,
  HOST    = 126
};

/* list of ICEInfo (candidates) is send during INVITE */
struct ICEInfo
{
  QString foundation;
  uint8_t component;   /* 1 to 256 */
  QString transport;   /* UDP/TCP  */
  int priority;

  QString address;
  quint16 port;

  QString type;        /* host, prflx, srflx or relayed */
  QString rel_address; /* for relay, srflx and prflx */
  quint16 rel_port;
};

struct ICEPair
{
  std::shared_ptr<ICEInfo> local;
  std::shared_ptr<ICEInfo> remote;
  uint64_t priority;
  PairState state;
};
