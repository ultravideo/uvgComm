Kvazzup
=======

Kvazzup is written in C++ and built on Qt framework. Kvazzup also makes use of the following open-source tools: Kvazaar for HEVC encoding, OpenHEVC for HEVC decoding, Opus Codec for audio encoding and decoding, and Live555 Streaming Media for RTP/RTCP traffic management. Currently, Kvazzup operates on Windows and it can be compiled using MinGW.

## Features 

Kvazzup can be used to make video calls based on an IP. Kvazzup supports saving IP:s to a contact list that is used to make calls. Some parameters of the call can be altered even during the call. Microphone and camera can be turned during a call. Supports multiple simultanious calls, but not yet as a conference. Kvazzup has a statistics window for viewing details about the call.

## Compile Kvazzup

Kvazzup needs the following external libraries to run:  
Kvazaar,  
Opus,  
OpenHEVC,  
LiveMedia,  
Speex ( for AEC )  

You have to build these libraries using MinGW. The liveMedia produced the most trouble. A packet with all required libraries may be provided in the future.

Qt Creator is recommended tool for compiling Kvazzup. The MinGW Toolkit has to be set and the build dir has to be the root folder.

## How to Run

You need a webcam and a microphone. You will also need to open the firewall port 5080 for SIP and 18888-18892 for media in the windows firewall and in any other firewalls if you want to call another computer.

## Future 

STUN, ICE and Video Conferencing are among planned features as well as support for more platforms.