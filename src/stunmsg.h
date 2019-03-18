#pragma once

#include <QtEndian>
#include <QByteArray>
#include <QHostAddress>

#include <vector>
#include <utility>

const uint32_t STUN_MAGIC_COOKIE = 0x2112A442;
const int TRANSACTION_ID_SIZE    = 12;

enum STUN_TYPES
{
  STUN_REQUEST  = 0x0001,
  STUN_RESPONSE = 0x0101
};

enum STUN_ATTRIBUTES
{
  STUN_ATTR_XOR_MAPPED_ADDRESS = 0x0020,
  STUN_ATTR_PRIORITY           = 0x0024,
  STUN_ATTR_USE_CANDIATE       = 0x0025,
  STUN_ATTR_ICE_CONTROLLED     = 0x8029,
  STUN_ATTR_ICE_CONTROLLING    = 0x802A,
};

struct STUNMessage
{
  uint16_t type;
  uint16_t length;
  uint32_t magicCookie;
  uint8_t transactionID[TRANSACTION_ID_SIZE];
  uint8_t payload[0];
  /* std::vector<uint16_t> attributes_; */
};

struct STUNRawMessage
{
  uint16_t type;
  uint16_t length;
  uint32_t magicCookie;
  uint8_t transactionID[TRANSACTION_ID_SIZE];
  unsigned char payload[0];
};

class STUNMessage_
{
public:
  STUNMessage_();
  STUNMessage_(uint16_t type, uint16_t length);
  STUNMessage_(uint16_t type);
  ~STUNMessage_();

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

  // get all message's attributes
  std::vector<uint16_t>& getAttributes();

  void addAttribute(uint16_t attribute);
  void addAttributeValue(uint16_t attribute, uint16_t value);

  // return true if the message contains xor-mapped-address and false if it doesn't
  // copy the address to info if possible
  bool getXorMappedAddress(std::pair<QHostAddress, uint16_t>& info);
  void setXorMappedAddress(QHostAddress address, uint16_t port);

private:
  uint16_t type_;
  uint16_t length_;
  uint32_t magicCookie_;
  uint8_t transactionID_[TRANSACTION_ID_SIZE];
  std::vector<uint16_t> attributes_;
  std::pair<QHostAddress, uint16_t> mappedAddr_;
};

class StunMessageFactory
{
public:
  StunMessageFactory();
  ~StunMessageFactory();

  // Create STUN Binding Request message
  STUNMessage_ createRequest();

  // Create empty (no transactionID) STUN Binding response message
  STUNMessage_ createResponse();

  // Create STUN Binding response message
  STUNMessage_ createResponse(STUNMessage_& request);

  // Convert from little endian to big endian and return the given STUN message as byte array
  QByteArray hostToNetwork(STUNMessage_& message);

  // Conver from big endian to little endian and return the byte array as STUN message
  STUNMessage_ networkToHost(QByteArray& message);

  // return true if the saved transaction id and message's transaction id match
  bool verifyTransactionID(STUNMessage_& message);

  bool validateStunResponse(STUNMessage_& message);
  bool validateStunRequest(STUNMessage_& message);

private:
  // validate all fields of STUNMessage_
  //
  // return true if message is valid, otherwise false
  bool validateStunMessage(STUNMessage_& message, int type);

  // return the next attribute-value pair
  std::pair<uint16_t, uint16_t> getAttribute(uint16_t *ptr);
};
