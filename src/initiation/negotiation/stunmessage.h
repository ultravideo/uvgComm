#pragma once

#include <QtEndian>
#include <QByteArray>
#include <QHostAddress>

#include <vector>
#include <tuple>
#include <utility>
#include <memory>

const int TRANSACTION_ID_SIZE    = 12;
const uint32_t STUN_MAGIC_COOKIE = 0x2112A442;

enum STUN_TYPES
{
  STUN_REQUEST  = 0x0001,
  STUN_RESPONSE = 0x0101,
  STUN_INVALID  = 0xffff,
};

enum STUN_ATTRIBUTES
{
  STUN_ATTR_XOR_MAPPED_ADDRESS = 0x0020,
  STUN_ATTR_PRIORITY           = 0x0024,
  STUN_ATTR_USE_CANDIATE       = 0x0025,
  STUN_ATTR_ICE_CONTROLLED     = 0x8029,
  STUN_ATTR_ICE_CONTROLLING    = 0x802A,
};

struct STUNRawMessage
{
  uint16_t type;
  uint16_t length;
  uint32_t magicCookie;
  uint8_t transactionID[TRANSACTION_ID_SIZE];
  unsigned char payload[256];
};

class STUNMessage
{
public:
  STUNMessage();
  STUNMessage(uint16_t type, uint16_t length);
  STUNMessage(uint16_t type);
  ~STUNMessage();

  void setType(uint16_t type);
  void setLength(uint16_t length);
  void setCookie(uint32_t cookie);

  void setTransactionID();
  void setTransactionID(uint8_t *transactionID);

  uint16_t getType();
  uint16_t getLength();
  uint32_t getCookie();

  // return pointer to message's transactionID array
  uint8_t *getTransactionID();
  uint8_t getTransactionIDAt(int index);

  // get all message's attributes
  /* std::vector<std::pair<uint16_t, uint16_t>>& getAttributes(); */
  std::vector<std::tuple<uint16_t, uint16_t, uint32_t>>& getAttributes();

  void addAttribute(uint16_t attribute);
  void addAttribute(uint16_t attribute, uint32_t value);

  // check if message has an attribute named "attrName" set
  // return true if yes and false if not
  bool hasAttribute(uint16_t attrName);

  // return true if the message contains xor-mapped-address and false if it doesn't
  // copy the address to info if possible
  bool getXorMappedAddress(std::pair<QHostAddress, uint16_t>& info);
  void setXorMappedAddress(QHostAddress address, uint16_t port);

private:
  uint16_t type_;
  uint16_t length_;
  uint32_t magicCookie_;
  uint8_t transactionID_[TRANSACTION_ID_SIZE];
  std::vector<std::tuple<uint16_t, uint16_t, uint32_t>> attributes_;
  std::pair<QHostAddress, uint16_t> mappedAddr_;
};
