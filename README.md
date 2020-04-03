Kvazzup
=======

Kvazzup is an HEVC video call software. Kvazzup is written in C++ and built on Qt framework. Kvazzup also makes use of the following open-source tools: Kvazaar for HEVC encoding, OpenHEVC for HEVC decoding, Opus Codec for audio encoding and decoding, and kvzRTP for Streaming Media. Currently, Kvazzup operates on Windows and it can be compiled using MinGW or Visual Studio. 

## Features 

Kvazzup has the following features:  
- SIP server integration for iniating a call with anyone who has registered to the server.
- Peer-to-peer call intiation (firewall needs to be open).
- Peer-to-peer media transport.
- Contact list.  
- Enable/disable audio or video.  
- Screen sharing.
- Live call parameter adjustment. 
- A statistics window.


## Compile Kvazzup

Kvazzup requires the following external libraries to run:  
- [Kvazaar](https://github.com/ultravideo/kvazaar) for video encoding.
- [OpenHEVC](https://github.com/OpenHEVC/openHEVC) for video decoding.
- [Opus](http://opus-codec.org/) for audio encoding and decoding.
- [kvzRTP](https://github.com/ultravideo/kvzRTP) for Media Delivery.
- [Speex DSP](https://www.speex.org/) for AEC.

Build these libraries using MinGW or Visual Studio. Qt Creator is recommended tool for compiling Kvazzup.


## Planned features

- Authentication
- Contact precense monitoring
- Video conferences
- Encryption
- Linux support
- Automatic call parameter adjustment
