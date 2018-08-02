Kvazzup
=======

Kvazzup is an HEVC video call software. Kvazzup is written in C++ and built on Qt framework. Kvazzup also makes use of the following open-source tools: Kvazaar for HEVC encoding, OpenHEVC for HEVC decoding, Opus Codec for audio encoding and decoding, and Live555 Streaming Media for RTP/RTCP traffic management. Currently, Kvazzup operates on Windows and it can be compiled using MinGW or Visual Studio.

## Features 

Kvazzup has the following features:  
- Make video calls based on an IP.  
- Contact list.  
- Enable/disable audio or video.  
- Live call parameter adjustment. 
- A statistics window.

## Compile Kvazzup

Kvazzup requires the following external libraries to run:  
- [Kvazaar](https://github.com/ultravideo/kvazaar) for video encoding.
- [OpenHEVC](https://github.com/OpenHEVC/openHEVC) for video decoding.
- [Opus](http://opus-codec.org/) for audio encoding and decoding.
- [LiveMedia](http://www.live555.com/liveMedia/) for Media Delivery.
- [Speex](https://www.speex.org/) for AEC.

Build these libraries using MinGW or Visual Studio. Qt Creator is recommended tool for compiling Kvazzup.

## How to Run

You need a webcam and a microphone. You will also need to open the firewall port 5080 for SIP and 8 ports per call from port 21500 upwards for media.
