#include <QDateTime>
#include <QDebug>
#include <memory>

#include "stunmsg.h"

STUNMessage_::STUNMessage_():
  type_(0),
  length_(0),
  magicCookie_(STUN_MAGIC_COOKIE)
{
  mappedAddr_.first  = QHostAddress("");
  mappedAddr_.second = 0;
}

STUNMessage_::STUNMessage_(uint16_t type, uint16_t length):
  STUNMessage_()
{
  type_ = type;
  length_ = length;
}

STUNMessage_::STUNMessage_(uint16_t type):
  STUNMessage_()
{
  type_ = type;
}

STUNMessage_::~STUNMessage_()
{
}

void STUNMessage_::setType(uint16_t type)
{
  this->type_ = type;
}

void STUNMessage_::setLength(uint16_t length)
{
  this->length_ = length;
}

void STUNMessage_::setCookie(uint32_t cookie)
{
  this->magicCookie_ = cookie;
}

void STUNMessage_::setTransactionID()
{
  for (int i = 0; i < TRANSACTION_ID_SIZE; ++i)
  {
    this->transactionID_[i] = uint8_t(qrand() % 256);
  }
}

void STUNMessage_::setTransactionID(uint8_t *transactionID)
{
  if (!transactionID)
  {
    return;
  }

  for (int i = 0; i < TRANSACTION_ID_SIZE; ++i)
  {
    this->transactionID_[i] = transactionID[i];
  }
}

void STUNMessage_::addAttribute(uint16_t attribute)
{
  this->length_ += 2;
  this->attributes_.push_back(attribute);
}

void STUNMessage_::addAttributeValue(uint16_t attribute, uint16_t value)
{
  this->length_ += 4;
  this->attributes_.push_back(attribute);
  this->attributes_.push_back(value);
}

uint16_t STUNMessage_::getType()
{
  return this->type_;
}

uint8_t *STUNMessage_::getTransactionID()
{
  return this->transactionID_;
}

uint16_t STUNMessage_::getLength()
{
  return this->length_;
}

uint32_t STUNMessage_::getCookie()
{
  return this->magicCookie_;
}

std::vector<uint16_t>& STUNMessage_::getAttributes()
{
  return this->attributes_;
}

bool STUNMessage_::getXorMappedAddress(std::pair<QHostAddress, uint16_t>& info)
{
  if (this->mappedAddr_.first == QHostAddress("") || this->mappedAddr_.second == 0)
  {
    return false;
  }

  info.first = this->mappedAddr_.first;
  info.second = this->mappedAddr_.second;

  return true;
}

void STUNMessage_::setXorMappedAddress(QHostAddress address, uint16_t port)
{
  this->mappedAddr_.first  = address;
  this->mappedAddr_.second = port;
}

// ----------------------------------------------------------------------------------------

StunMessageFactory::StunMessageFactory()
{
  qsrand(QDateTime::currentSecsSinceEpoch() / 2 + 1);
}

StunMessageFactory::~StunMessageFactory()
{
}

STUNMessage_ StunMessageFactory::createRequest()
{
  STUNMessage_ request(STUN_REQUEST);
  
  return request;
}

STUNMessage_ StunMessageFactory::createResponse()
{
  STUNMessage_ response(STUN_RESPONSE);

  return response;
}

STUNMessage_ StunMessageFactory::createResponse(STUNMessage_& request)
{
  STUNMessage_ response(STUN_RESPONSE);

  response.setTransactionID(request.getTransactionID());
  return response;
}

QByteArray StunMessageFactory::hostToNetwork(STUNMessage_& message)
{
  STUNRawMessage rawMessage;

  rawMessage.type        = qToBigEndian((short)message.getType());
  rawMessage.length      = qToBigEndian(message.getLength());
  rawMessage.magicCookie = qToBigEndian(message.getCookie());

  for (int i = 0; i < TRANSACTION_ID_SIZE; ++i)
  {
    rawMessage.transactionID[i] = qToBigEndian(message.getTransactionID()[i]);
  }

  memcpy(&rawMessage.transactionID, &rawMessage.transactionID, sizeof(rawMessage.transactionID));

  auto rawMem = std::make_unique<char>(sizeof(STUNRawMessage));
  memcpy(rawMem.get(), &rawMessage, sizeof(STUNRawMessage));

  return QByteArray(rawMem.get(), sizeof(STUNRawMessage));
}

STUNMessage_ StunMessageFactory::networkToHost(QByteArray& message)
{
  int i = 0;
  char *raw_data = message.data();
  STUNMessage_ response(STUN_RESPONSE);

  response.setType(qFromBigEndian(*((uint16_t *)&raw_data[0])));
  response.setLength(qFromBigEndian(*((uint16_t *)&raw_data[2])));
  response.setCookie(qFromBigEndian(*((uint32_t *)&raw_data[4])));

  for (i = 0; i < TRANSACTION_ID_SIZE; ++i)
  {
    // TODO ugly
    response.getTransactionID()[i] = (uint8_t)raw_data[8 + i];
  }

  // parse payload (extract attributes and xor-mapped-address)
  std::pair<uint16_t, uint16_t> attrPair;
  uint16_t *payload  = (uint16_t *)((raw_data + 8 + i));

  for (int k = 0; k < response.getLength(); ++k)
  {
    attrPair = getAttribute(payload);
    payload += 2; k += 2;

    switch (attrPair.first)
    {
      // TODO ugly, refactor asap
      case STUN_ATTR_XOR_MAPPED_ADDRESS:
        if (attrPair.second >= 8)
        {
          if ((((*payload) >> 8) & 0xff) == 0x01)
          {
            payload++;

            uint16_t port    = qFromBigEndian(*payload) ^ 0x2112;

            payload++;

            uint32_t address = qFromBigEndian(*(uint32_t *)payload) ^ response.getCookie();

            response.setXorMappedAddress(QHostAddress(address), port);
            k += attrPair.second + 1;
          }
        }
        break;

      case STUN_ATTR_ICE_CONTROLLED:
        response.addAttribute(STUN_ATTR_ICE_CONTROLLED);
        break;

      case STUN_ATTR_ICE_CONTROLLING:
        response.addAttribute(STUN_ATTR_ICE_CONTROLLING);
        break;

      case STUN_ATTR_PRIORITY:
        response.addAttribute(STUN_ATTR_PRIORITY);
        break;

      case STUN_ATTR_USE_CANDIATE:
        response.addAttribute(STUN_ATTR_USE_CANDIATE);
        break;

      default:
        fprintf(stderr, "Invalid attribute: 0x%x\n", attrPair.first);
        break;
    }
  }

  return response;
}

bool StunMessageFactory::verifyTransactionID(STUNMessage_& message)
{
  return false;
}

std::pair<uint16_t, uint16_t> StunMessageFactory::getAttribute(uint16_t *ptr)
{
  if (!ptr)
  {
    return std::make_pair(0, 0);
  }

  uint16_t attr    = ptr[0];
  uint16_t attrLen = ptr[1];

  return std::make_pair(qFromBigEndian(attr), qFromBigEndian(attrLen));
}

bool StunMessageFactory::validateStunMessage(STUNMessage_& message, int type)
{
  if (message.getCookie() != STUN_MAGIC_COOKIE)
  {
    return false;
  }

  return true;
}

bool StunMessageFactory::validateStunRequest(STUNMessage_& message)
{
  return this->validateStunMessage(message, STUN_REQUEST);
}

bool StunMessageFactory::validateStunResponse(STUNMessage_& message)
{
  return this->validateStunMessage(message, STUN_RESPONSE);
}
