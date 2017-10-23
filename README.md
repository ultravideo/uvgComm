Kvazzup
=======

Kvazzup is written in C++ and built on Qt framework. Kvazzup also makes use of the following open-source tools: Kvazaar for HEVC encoding, OpenHEVC for HEVC decoding, Opus Codec for audio encoding and decoding, and Live555 Streaming Media for RTP/RTCP traffic management. Currently, Kvazzup operates on Windows and it can be compiled using MinGW.

## Features 

Kvazzup has the following features:  
- Make video calls based on an IP.  
- Contact list.  
- Enable/disable audio or video.  
- Live call paramter adjustement.  
- A statistics window.

## Compile Kvazzup

Kvazzup needs the following external libraries to run:  
- Kvazaar  
- Opus  
- OpenHEVC  
- LiveMedia  
- Speex

Build these libraries using MinGW. A packet with all required libraries may be provided in the future. Qt Creator is recommended tool for compiling Kvazzup.

## How to Run

You need a webcam and a microphone. You will also need to open the firewall port 5080 for SIP and 18888-18892 for media in the windows firewall and in any other firewalls if you want to call another computer. Opening the ports will not be needed once STUN has been implemented.