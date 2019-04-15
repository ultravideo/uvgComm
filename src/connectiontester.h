#pragma once

#include <QThread>
#include "icetypes.h"
#include "udpserver.h"
#include "stun.h"

class ConnectionTester : public QThread
{
  Q_OBJECT

public:
    ConnectionTester();
    ~ConnectionTester();
    void setStun(Stun *stun);
    void setCandidatePair(ICEPair *pair);

    // controller_ defines the course of action after candiate pair has been validated.
    // If the ConnectionTester belongs to FlowController it terminates immediately after
    // confirming the validity of pair and FlowController can start the nomination process
    //
    // If the owner is instead FlowControllee, ConnectionTester start responding to nomination
    // requests after it has concluded the candidate verification
    void isController(bool controller);

    void printMessage(QString message);

signals:
    // testingDone() is emitted when the connection testing has ended
    //
    // if the tested candidate succeeded (remote responded to our requests),
    // connection points to valid ICEPair, otherwise it's nullptr
    void testingDone(ICEPair *connection);

protected:
    void run();

    ICEPair *pair_;
    bool controller_;
    Stun *stun_;
};
