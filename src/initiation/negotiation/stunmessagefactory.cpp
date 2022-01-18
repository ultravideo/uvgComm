#include "stunmessagefactory.h"

#include "logger.h"
#include "common.h"

#include <QDateTime>

#include <cstdlib>

// allocate 256 bytes of temporary memory for each outgoing STUN message
// Usually the size of message is less than 100 bytes but just in case add
// some wiggle room.
//
// This has to be done because 64-bit MinGW seems to call unique_ptr's
// *destructor* when allocating space for the message. Allocating buffer large
// enough fixes the problem
const int STUN_MSG_MAX_SIZE = 256;

StunMessageFactory::StunMessageFactory()
{
  srand(QDateTime::currentSecsSinceEpoch() / 2 + 1);
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

  response.setTransactionID(latestRequest_.getTransactionID());
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
  (void)message;

  return false;
}

bool StunMessageFactory::validateStunMessage(STUNMessage& message, int type)
{
  if (message.getCookie() != STUN_MAGIC_COOKIE)
  {
    Logger::getLogger()->printDebug(DEBUG_WARNING, "StunMessageFactory", 
                                    "Did not include magic cookie");
    return false;
  }

  if (message.getType() != type)
  {
    Logger::getLogger()->printDebug(DEBUG_WARNING, "StunMessageFactory", 
                                    "Request/response type mismatch");
    return false;
  }

  return true;
}

bool StunMessageFactory::validateStunRequest(STUNMessage& message)
{
  return this->validateStunMessage(message, STUN_REQUEST);
}

bool StunMessageFactory::validateStunResponse(STUNMessage& response,
                                              QHostAddress sender,
                                              uint16_t port)
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
              Logger::getLogger()->printDebug(DEBUG_WARNING, "StunMessageFactory",
                                             "Incorrect response transaction ID!");
              return false;
            }
        }
        return true;
    }
    else
    {
      Logger::getLogger()->printDebug(DEBUG_WARNING, "StunMessageFactory", 
                                      "Port not reported!");
    }
  }

  // expected response address:port was not saved for whatever reason
  // check the received response against the latest request
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
        Logger::getLogger()->printDebug(DEBUG_WARNING, "StunMessageFactory",
                   "Incorrect response transaction ID!");
        return false;
      }
    }

    return true;
  }

  return false;
}

void StunMessageFactory::expectReplyFrom(
    STUNMessage& request,
    QString address,
    uint16_t port
)
{
  if (expectedResponses_.contains(address))
  {
    if (expectedResponses_[address].contains(port))
    {
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
  const int MSG_SIZE = sizeof(STUNRawMessage) - 256 + message.getLength();

  auto ptr = std::unique_ptr<unsigned char[]>{
    new unsigned char[STUN_MSG_MAX_SIZE]};

  STUNRawMessage *rawMessage =
    static_cast<STUNRawMessage *>(static_cast<void *>(ptr.get()));

  rawMessage->type        = qToBigEndian((short)message.getType());
  rawMessage->length      = qToBigEndian(message.getLength());
  rawMessage->magicCookie = qToBigEndian(message.getCookie());

  memcpy(rawMessage->transactionID, message.getTransactionID(), TRANSACTION_ID_SIZE);

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

  return QByteArray(static_cast<const char *>(
        static_cast<void *>(ptr.get())), MSG_SIZE);
}

bool StunMessageFactory::networkToHost(QByteArray& message, STUNMessage& outSTUN)
{
  if (message.size() < 20)
  {
    Logger::getLogger()->printDebug(DEBUG_WARNING, "StunMessageFactory",
                                    "Received too small packet to be a STUN message");
    return false;
  }

  char *raw_data = message.data();

  outSTUN.setType(qFromBigEndian(*((uint16_t *)&raw_data[0])));
  outSTUN.setLength(qFromBigEndian(*((uint16_t *)&raw_data[2])));
  outSTUN.setCookie(qFromBigEndian(*((uint32_t *)&raw_data[4])));

  memcpy(outSTUN.getTransactionID(), (uint8_t *)raw_data + 8, TRANSACTION_ID_SIZE);

  if (outSTUN.getLength() + 8 + TRANSACTION_ID_SIZE == message.size())
  {
    uint32_t *payload  = (uint32_t *)((uint8_t *)raw_data + 8 + TRANSACTION_ID_SIZE);
    uint32_t length    = outSTUN.getLength();

    for (size_t i = 0; i < length / 4; ++i) {
      uint32_t value    = qFromBigEndian(payload[i]);
      uint16_t attrName = (value >> 16) & 0xffff;
      uint16_t attrLen  = (value >>  0) & 0xffff;

      switch (attrName)
      {
        case STUN_ATTR_XOR_MAPPED_ADDRESS:
        {
          auto xorMappedAddr = extractXorMappedAddress(attrLen, (uint8_t *)payload + 4);
          if (xorMappedAddr.second != 0)
          {
            outSTUN.setXorMappedAddress(xorMappedAddr.first, xorMappedAddr.second);
          }
          break;
        }
        case STUN_ATTR_ICE_CONTROLLING:
        {
          outSTUN.addAttribute(STUN_ATTR_ICE_CONTROLLING);
          break;
        }
        case STUN_ATTR_ICE_CONTROLLED:
        {
          outSTUN.addAttribute(STUN_ATTR_ICE_CONTROLLED);
          break;
        }
        case STUN_ATTR_PRIORITY:
        {
          uint32_t priority = qFromBigEndian(payload[i + 1]);
          outSTUN.addAttribute(STUN_ATTR_PRIORITY, priority);
          break;
        }
        case STUN_ATTR_USE_CANDIDATE:
        {
          outSTUN.addAttribute(STUN_ATTR_USE_CANDIDATE);
          break;
        }
        default:
        {
          Logger::getLogger()->printDebug(DEBUG_WARNING, "StunMessageFactory", 
                                          "Could not determine attribute!");
          return false;
          break;
        }
      }

      i += (attrLen / 4);
    }
  }
  else
  {
    Logger::getLogger()->printDebug(DEBUG_WARNING, "StunMessageFactory",
                                    "STUN message length does not match packet size");
    return false;
  }

  return true;
}

std::pair<QHostAddress, uint16_t> StunMessageFactory::extractXorMappedAddress(
  uint16_t payloadLen, uint8_t *payload)
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
    else if (payload[1] == 0x02) // IPv6
    {
      Logger::getLogger()->printDebug(DEBUG_PROGRAM_WARNING, "StunMessageFactory", 
                                      "IPv6 Unimplemented");
    }
  }

  return std::make_pair(QHostAddress(""), 0);
}
