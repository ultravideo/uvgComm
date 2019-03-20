#include <QDateTime>
#include <QDebug>
#include <memory>

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

// ----------------------------------------------------------------------------------------

StunMessageFactory::StunMessageFactory()
{
  qsrand(QDateTime::currentSecsSinceEpoch() / 2 + 1);
}

StunMessageFactory::~StunMessageFactory()
{
}

STUNMessage StunMessageFactory::createRequest()
{
  STUNMessage request(STUN_REQUEST);

  request.setTransactionID();
  return request;
}

STUNMessage StunMessageFactory::createResponse()
{
  STUNMessage response(STUN_RESPONSE);

  return response;
}

STUNMessage StunMessageFactory::createResponse(STUNMessage& request)
{
  STUNMessage response(STUN_RESPONSE);

  response.setTransactionID(request.getTransactionID());
  return response;
}

QByteArray StunMessageFactory::hostToNetwork(STUNMessage& message)
{
  auto attrs = message.getAttributes();
  const size_t MSG_SIZE = sizeof(STUNRawMessage) + message.getLength();

  auto ptr = std::unique_ptr<unsigned char[]>{ new unsigned char[MSG_SIZE] };
  STUNRawMessage *rawMessage = static_cast<STUNRawMessage *>(static_cast<void *>(ptr.get()));

  rawMessage->type        = qToBigEndian((short)message.getType());
  rawMessage->length      = qToBigEndian(message.getLength());
  rawMessage->magicCookie = qToBigEndian(message.getCookie());

  for (int i = 0; i < TRANSACTION_ID_SIZE; ++i)
  {
    rawMessage->transactionID[i] = qToBigEndian(message.getTransactionID()[i]);
  }

  uint16_t *attrPtr = (uint16_t *)rawMessage->payload;

  for (size_t i = 0, k = 0; i < attrs.size(); ++i, k += 2)
  {
    attrPtr[k + 0] = qToBigEndian(std::get<0>(attrs[i]));
    attrPtr[k + 1] = qToBigEndian(std::get<1>(attrs[i]));

    if (std::get<1>(attrs[i]) > 0)
    {
      ((uint32_t *)attrPtr)[k + 1] = qToBigEndian(std::get<2>(attrs[i]));
      k += 2;
    }
  }

  return QByteArray(static_cast<const char *>(static_cast<void *>(ptr.get())), MSG_SIZE);
}

STUNMessage StunMessageFactory::networkToHost(QByteArray& message)
{
  int i = 0;
  char *raw_data = message.data();
  STUNMessage response(STUN_RESPONSE);

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
        k += 2, payload += 2;
        response.addAttribute(STUN_ATTR_PRIORITY);
        break;

      case STUN_ATTR_USE_CANDIATE:
        response.addAttribute(STUN_ATTR_USE_CANDIATE);
        break;

      default:
        // TODO handle invalid message?
        break;
    }
  }

  return response;
}

bool StunMessageFactory::verifyTransactionID(STUNMessage& message)
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

bool StunMessageFactory::validateStunMessage(STUNMessage& message, int type)
{
  if (message.getCookie() != STUN_MAGIC_COOKIE)
  {
    return false;
  }

  return true;
}

bool StunMessageFactory::validateStunRequest(STUNMessage& message)
{
  return this->validateStunMessage(message, STUN_REQUEST);
}

bool StunMessageFactory::validateStunResponse(STUNMessage& message)
{
  return this->validateStunMessage(message, STUN_RESPONSE);
}
