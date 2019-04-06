#pragma once

#include <QThread>
#include "icetypes.h"

class ConnectionTester : public QThread
{
  Q_OBJECT

public:
    ConnectionTester();
    ~ConnectionTester();
    void setCandidatePair(ICEPair *pair_rtp, ICEPair *pair_rtcp);

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
    // rtp and rtcp point to valid ICEPairs
    //
    // if something failed, rtp and rtcp are nullptr
    void testingDone(ICEPair *rtp, ICEPair *rtcp);

protected:
    void run();

    ICEPair *rtp_pair_;
    ICEPair *rtcp_pair_;

    bool controller_;
};
