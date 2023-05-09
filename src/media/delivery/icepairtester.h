#pragma once

#include "icetypes.h"
#include "udpserver.h"
#include "stunmessagefactory.h"

#include <QThread>
#include <memory>

/* Test one possible connection between one local pair and one remote pair.
 * Note that one local pair has one ICEPairTester for each remote candidate.
 */

class ICEPairTester : public QThread
{
  Q_OBJECT

public:
  ICEPairTester(UDPServer *server);
  ~ICEPairTester();
  void setCandidatePair(std::shared_ptr<ICEPair> pair);

  // Whether we are the controller affects how testing is performed. Most
  // importantly non-controller waits for nomination after successful binding
  // while controller exits and performs nomination later.
  void setController(bool controller);

  // Send the nominated candidate to ICE_CONTROLLED agent
  bool sendNominationWaitResponse(ICEPair *pair);

  void recvStunMessage(QNetworkDatagram message);

  // helper functions that get either actual address/port or
  // relay address/port if needed
  QHostAddress getLocalAddress(std::shared_ptr<ICEInfo> info);
  quint16 getLocalPort(std::shared_ptr<ICEInfo> info);

public slots:
  // Call this when you want to stop testing.
  // Also stops eventloop.
  void quit();

signals:

  void controllerPairSucceeded(std::shared_ptr<ICEPair> connection);
  void controlleeNominationDone(std::shared_ptr<ICEPair> connection);

  void responseRecv();
  void nominationRecv();
  void requestRecv();
  void stopEventLoop();

protected:
  void printMessage(QString message);
  void run();

private:

  // Send STUN Binding Response to remote. Each message should be response to alraedy-received
  // request and this is why the request we're responding to is given as parameter
  //
  // TransactionID from request is copied to response
  bool sendBindingResponse(STUNMessage& request, QString addressRemote, int portRemote);


  // this function is called by the ICE_CONTROLLED agent when the two clients have
  // negotiated a list of possible candidates.
  // The ICE_CONTROLLING agent selects component candidates used for media streaming
  // and sends these candidates using STUN Binding request
  //
  // ICE_CONTROLLED agent (caller of sendNominationResponse()) must send STUN Binding Requests
  // to all valid candidate address:port pairs to receive the nominations
  bool waitNominationSendResponse(ICEPair *pair);

  bool waitForStunRequest(unsigned long timeout);
  bool waitForStunResponse(unsigned long timeout);
  bool waitForStunNomination(unsigned long timeout);

  // waitForStunMessage() starts an even loop which listens to request, response and nomination signals
  // When the signal is received the event loop is stopped and waitForStunMessage() returns true
  // If responseRecv() signal is not received in time, waitForStunMessage() returns false
  bool waitForStunMessage(unsigned long timeout,
                          bool expectingRequest,
                          bool expectingResponse,
                          bool expectingNomination);


  // If we're the controlling agent we start by sending STUN Binding Requests to remote
  // When we receive a response to one of our STUN Binding Requests, we start listening to
  // remote's STUN Binding Requests. When we receive one, we acknowledge it by sending STUN
  // Binding Response to remote.
  // Now the we've concluded that the communication works and we can stop testing.
  //
  // Return true if the testing succeeded, false otherwise
  bool controllerBinding(ICEPair *pair);

  // if we're the controlled agent, we must first start sending "dummy" binding requests to
  // make a hole in the firewall and after we've gotten a request from remote (controlling agent)
  // we can switch to sending responses

  // if we're the controlled agent, we start sending STUN Binding Requests to create a hole in the
  // firewall. When we get a response from remote (the controlling agent) we acknowledge it by sending
  // STUN Binding Request.
  //
  // After we've received a STUN Request from remote, we start listening to responses for our STUN Binding Requests.
  // When we've received a response we mark this pair as valid as we've established that communication works to both directions
  //
  // Return true if the testing succeeded, false otherwise
  bool controlleeBinding(ICEPair *pair);

  bool sendRequestWaitResponse(ICEPair *pair, QByteArray &request, int retries, int baseTimeout);

  QString stateToString(PairState state);


  std::shared_ptr<ICEPair> pair_;
  QString debugPair_;
  bool controller_;
  QString debugType_;

  UDPServer *udp_;

  StunMessageFactory stunOutTransaction_;
  StunMessageFactory stunInTransaction_;

  // When waitFor(Stun|Nomination)(Request|Response) returns, the calling code should
  // check whether interrupt flag has been set. It means that the running thread has
  // decided to terminate processing and we shouln't continue any further
  bool interrupted_;
};
