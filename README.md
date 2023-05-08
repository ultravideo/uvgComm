Kvazzup
=======

Kvazzup is a *High Efficiency Video Coding (HEVC)* video call software written in C++ and built on [Qt](https://www.qt.io/) application framework. The aim of Kvazzup is to be pave way for better quality video calls while valuing usability, security and privacy. 

Kvazzup is under development and new features will become available.

## Current features 

**Protocols**
- Signaling with *Session Initiation Protocol (SIP)*
- Negotiation with *Session Description Protocol (SDP)*
- Connectivity with *Interactive Connectivity Establishment (ICE)*
- Delivery with *Real-Time Transport Protocol (RTP)*

**Media**
- HEVC codec for video
- Opus codec for audio
- Support for 13 different camera input pixel formats
- Option to disable audio and/or video
- Screen sharing

**Analytics and Settings**
- A statistics window for monitoring call quality
- Full customizability of both audio and video processing in settings
- Automatic selection of best media settings with option for live manual adjustment
- All settings are recorded to the disk and loaded at startup

**Other**
- Contacts list

See [FEATURES.md](FEATURES.md) for more detailed description of features.

## Building Kvazzup

Kvazzup relies on following external libraries: 
- [Kvazaar](https://github.com/ultravideo/kvazaar) for HEVC encoding
- [OpenHEVC](https://github.com/OpenHEVC/openHEVC) for HEVC decoding
- [libyuv](https://chromium.googlesource.com/libyuv/libyuv/) for video input processing
- [Opus](http://opus-codec.org/) for audio coding
- [Speex DSP](https://www.speex.org/) for audio processing
- [uvgRTP](https://github.com/ultravideo/uvgRTP) for media delivery
- [Crypto++](https://cryptopp.com/) for delivery encryption (optional)

Kvazzup uses CMake to build itself and missing dependencies with minimal effort from the developer, see [BUILDING.md](BUILDING.md) for build instructions.

## Setting up a SIP Server

Kvazzup has been tested together with *Kamailio Open Source SIP Server*. Easy to follow instructions are hard to come by on the internet, so we provide our own [instructions](kamailio/README.md) for setting up Kamailio.

## Papers

If you are using Kvazzup in your research, please cite one of the following papers: <br>

[Kvazzup: open software for HEVC video calls](https://urn.fi/URN:NBN:fi:tty-201908262019)

`J. Räsänen, M. Viitanen, J. Vanne, and T. D. Hämäläinen, “Kvazzup: open software for HEVC video calls,” in Proc. IEEE Int. Symp. Multimedia, Taichung, Taiwan, Dec. 2017. `

[Live Demonstration: Kvazzup 4K HEVC Video Call](https://urn.fi/URN:NBN:fi:tty-201908231999)

`J. Räsänen, M. Viitanen, J. Vanne, and T. D. Hämäläinen, “Live Demonstration: Kvazzup 4K HEVC Video Call,” in Proc. IEEE Int. Symp. Multimedia, Taichung, Taiwan, Dec. 2018. `

[Live Demonstration : Interactive Quality of Experience Evaluation in Kvazzup Video Call](https://urn.fi/URN:NBN:fi:tuni-202102041923)

`J. Räsänen, A. Altonen, A. Mercat, and J. Vanne, “Live Demonstration : Interactive Quality of Experience Evaluation in Kvazzup Video Call,” in Proc. IEEE Int. Symp. Multimedia, Naples, Italy, Dec. 2020. `

## Planned features

- Contact presence monitoring
- Multiparty video conferences
- TLS Encryption for SIP
- Integrating SRTP & ZRTP