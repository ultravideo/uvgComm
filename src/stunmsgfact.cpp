#include "stunmsgfact.h"
#include <QDateTime>

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

bool StunMessageFactory::verifyTransactionID(STUNMessage& message)
{
  return false;
}

bool StunMessageFactory::validateStunMessage(STUNMessage& message, int type)
{
  if (message.getCookie() != STUN_MAGIC_COOKIE)
  {
    return false;
  }

  if (message.getType() != type)
  {
    return false;
  }

  return true;
}

bool StunMessageFactory::validateStunRequest(STUNMessage& message)
{
  return this->validateStunMessage(message, STUN_REQUEST);
}

bool StunMessageFactory::validateStunResponse(STUNMessage& response, QHostAddress sender, uint16_t port)
{
  if (expectedResponses_.contains(sender.toString()))
  {
      if (expectedResponses_[sender.toString()].contains(port))
      {
          auto cached = expectedResponses_[sender.toString()][port];

          for (int i = 0; i < TRANSACTION_ID_SIZE; ++i)
          {
              if (cached[i] != response.getTransactionIDAt(i))
              {
                return false;
              }
          }
          return true;
      }
    else
    {
      qDebug() << "port not reported";
    }
  }

  // expected response address:port was not saved for whatever reason
  // check the received response agains the latest request
  return this->validateStunResponse(response);
}

bool StunMessageFactory::validateStunResponse(STUNMessage& response)
{
  if (this->validateStunMessage(response, STUN_RESPONSE))
  {
    uint8_t *responseTID = response.getTransactionID();
    uint8_t *requestTID  = latestRequest_.getTransactionID();

    for (int i = 0; i < TRANSACTION_ID_SIZE; ++i)
    {
      if (responseTID[i] != requestTID[i])
      {
        return false;
      }
    }

    return true;
  }

  return false;
}

void StunMessageFactory::expectReplyFrom(STUNMessage& request, QString address, uint16_t port)
{
  if (expectedResponses_.contains(address))
  {
    if (expectedResponses_[address].contains(port))
    {
      qDebug() << "Purging old entry for " << address << ":" << port;
      expectedResponses_[address][port].clear();
    }
  }

  for (int i = 0; i < TRANSACTION_ID_SIZE; ++i)
  {
    expectedResponses_[address][port].push_back(request.getTransactionIDAt(i));
  }
}

void StunMessageFactory::cacheRequest(STUNMessage request)
{
  latestRequest_ = request;
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
    rawMessage->transactionID[i] = qToBigEndian(message.getTransactionIDAt(i));
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
  char *raw_data = message.data();
  STUNMessage response(STUN_RESPONSE);

  response.setType(qFromBigEndian(*((uint16_t *)&raw_data[0])));
  response.setLength(qFromBigEndian(*((uint16_t *)&raw_data[2])));
  response.setCookie(qFromBigEndian(*((uint32_t *)&raw_data[4])));

  for (int i = 0; i < TRANSACTION_ID_SIZE; ++i)
  {
    response.getTransactionID()[i] = (uint8_t)raw_data[8 + i];
  }

  std::pair<uint16_t, uint16_t> attrPair;
  uint16_t *payload  = (uint16_t *)((raw_data + 8 + TRANSACTION_ID_SIZE));

  for (int k = 0; k < response.getLength(); )
  {
    attrPair = getAttribute(payload + k);
    k += 2;

    switch (attrPair.first)
    {
      case STUN_ATTR_XOR_MAPPED_ADDRESS:
        {
          auto xorMappedAddr = extractXorMappedAddress(attrPair.second, (uint8_t *)payload + k * sizeof(uint16_t));
          if (xorMappedAddr.second != 0)
          {
            k += attrPair.second / sizeof(uint16_t);
            response.setXorMappedAddress(xorMappedAddr.first, xorMappedAddr.second);
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
        k += 2; // skip the priority (for now at least)
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

std::pair<QHostAddress, uint16_t> StunMessageFactory::extractXorMappedAddress(uint16_t payloadLen, uint8_t *payload)
{
  if (payloadLen >= 8)
  {
    // first byte (payload[0]) ignored according to RFC 5389
    if (payload[1] == 0x01) // IPv4
    {
      // RFC 5389 page 33
      uint16_t port    = qFromBigEndian(*(((uint16_t *)payload + 1))) ^ 0x2112;
      uint32_t address = qFromBigEndian(*(((uint32_t *)payload + 1))) ^ STUN_MAGIC_COOKIE;

      return std::make_pair(QHostAddress(address), port);
    }
  }

  return std::make_pair(QHostAddress(""), 0);
}
