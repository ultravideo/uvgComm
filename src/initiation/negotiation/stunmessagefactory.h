#pragma once

#include "stunmessage.h"

class StunMessageFactory
{
public:
  StunMessageFactory();
  ~StunMessageFactory();

  // Create STUN Binding Request message
  STUNMessage createRequest();

  // Create empty (no transactionID) STUN Binding response message
  STUNMessage createResponse();

  // Create STUN Binding response message
  STUNMessage createResponse(STUNMessage& request);

  // Convert from little endian to big endian and return the given STUN message as byte array
  QByteArray hostToNetwork(STUNMessage& message);

  // Conver from big endian to little endian and return the byte array as STUN message
  STUNMessage networkToHost(QByteArray& message);

  // return true if the saved transaction id and message's transaction id match
  bool verifyTransactionID(STUNMessage& message);

  bool validateStunRequest(STUNMessage& message);

  // validate stun response based on the latest request we have sent
  bool validateStunResponse(STUNMessage& response);

  // validate response based on 
  bool validateStunResponse(STUNMessage& response, QHostAddress sender, uint16_t port);

  // cache request to memory
  //
  // this request is used to verify the transactionID of received response
  void cacheRequest(STUNMessage request);

  // if we know we're going to receive response from some address:port combination
  // the transactionID can be saved to memory and then fetched for verification
  // when the response arrives
  //
  // This greatly improves the reliability of transactionID verification
  void expectReplyFrom(STUNMessage& request, QString address, uint16_t port);

private:
  // validate all fields of STUNMessage
  //
  // return true if message is valid, otherwise false
  bool validateStunMessage(STUNMessage& message, int type);

  // extract host address and port learned from STUN binding request to google server
  // this is just to make the code look nicer
  std::pair<QHostAddress, uint16_t> extractXorMappedAddress(uint16_t payloadLen, uint8_t *payload);

  // save transactionIDs by address and port
  QMap<QString, QMap<uint16_t, std::vector<uint8_t>>> expectedResponses_;

  STUNMessage latestRequest_;
};
