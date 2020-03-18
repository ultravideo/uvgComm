#include <QDateTime>

#include "stunmsg.h"

STUNMessage::STUNMessage():
  type_(0),
  length_(0),
  magicCookie_(STUN_MAGIC_COOKIE)
{
  mappedAddr_.first  = QHostAddress("");
  mappedAddr_.second = 0;
}

STUNMessage::STUNMessage(uint16_t type, uint16_t length):
  STUNMessage()
{
  type_ = type;
  length_ = length;
}

STUNMessage::STUNMessage(uint16_t type):
  STUNMessage()
{
  type_ = type;
}

STUNMessage::~STUNMessage()
{
}

void STUNMessage::setType(uint16_t type)
{
  this->type_ = type;
}

void STUNMessage::setLength(uint16_t length)
{
  this->length_ = length;
}

void STUNMessage::setCookie(uint32_t cookie)
{
  this->magicCookie_ = cookie;
}

void STUNMessage::setTransactionID()
{
  for (int i = 0; i < TRANSACTION_ID_SIZE; ++i)
  {
    this->transactionID_[i] = uint8_t(qrand() % 256);
  }
}

void STUNMessage::setTransactionID(uint8_t *transactionID)
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

void STUNMessage::addAttribute(uint16_t attribute)
{
  this->length_ += 2 * sizeof(uint16_t);
  this->attributes_.push_back(std::make_tuple(attribute, 0, 0));
}

void STUNMessage::addAttribute(uint16_t attribute, uint32_t value)
{
  this->length_ += 2 * sizeof(uint16_t) + sizeof(uint32_t);
  this->attributes_.push_back(std::make_tuple(attribute, 4, value));
}

uint16_t STUNMessage::getType()
{
  return this->type_;
}

uint8_t *STUNMessage::getTransactionID()
{
  return this->transactionID_;
}

uint8_t STUNMessage::getTransactionIDAt(int index)
{
  if (index >= 0 && index < TRANSACTION_ID_SIZE)
  {
    return this->transactionID_[index];
  }

  return 0;
}

uint16_t STUNMessage::getLength()
{
  return this->length_;
}

uint32_t STUNMessage::getCookie()
{
  return this->magicCookie_;
}

std::vector<std::tuple<uint16_t, uint16_t, uint32_t>>& STUNMessage::getAttributes()
{
  return this->attributes_;
}

bool STUNMessage::getXorMappedAddress(std::pair<QHostAddress, uint16_t>& info)
{
  if (this->mappedAddr_.first == QHostAddress("") || this->mappedAddr_.second == 0)
  {
    return false;
  }

  info.first = this->mappedAddr_.first;
  info.second = this->mappedAddr_.second;

  return true;
}

void STUNMessage::setXorMappedAddress(QHostAddress address, uint16_t port)
{
  this->mappedAddr_.first  = address;
  this->mappedAddr_.second = port;
}

bool STUNMessage::hasAttribute(uint16_t attrName)
{
  for (size_t i = 0; i < this->attributes_.size(); ++i)
  {
    if (std::get<0>(this->attributes_[i]) == attrName)
    {
      return true;
    }
  }

  return false;
}
