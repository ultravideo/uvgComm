uvgComm
=======

uvgComm is a *High Efficiency Video Coding (HEVC)* video call software written in C++ and built on [Qt](https://www.qt.io/) application framework. uvgComm aims to provide a testbed for novel video conferencing solutions, while also advancing the field of *Real-Time Communication (RTC)*. uvgComm is licensed under permissive ISC-license.

uvgComm was previously called Kvazzup. uvgComm is under development and new features will become available.

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

## Building uvgComm

uvgComm relies on following external libraries: 
- [Kvazaar](https://github.com/ultravideo/kvazaar) for HEVC encoding
- [OpenHEVC](https://github.com/OpenHEVC/openHEVC) for HEVC decoding
- [libyuv](https://chromium.googlesource.com/libyuv/libyuv/) for video input processing
- [Opus](http://opus-codec.org/) for audio coding
- [Speex DSP](https://www.speex.org/) for audio processing
- [uvgRTP](https://github.com/ultravideo/uvgRTP) for media delivery
- [Crypto++](https://cryptopp.com/) for delivery encryption (optional)

uvgComm uses CMake to build itself and missing dependencies with minimal effort from the developer, see [BUILDING.md](BUILDING.md) for build instructions.

## Setting up a SIP Server

uvgComm has been tested together with *Kamailio Open Source SIP Server*. Easy to follow instructions are hard to come by on the internet, so we provide our own [instructions](kamailio/README.md) for setting up Kamailio.

## Instructions

You can find instructions on how to use uvgComm [here](INSTRUCTIONS.md).

## Papers

If you are using uvgComm in your research, please cite one of the following papers: <br>

[Kvazzup: open software for HEVC video calls](https://researchportal.tuni.fi/en/publications/kvazzup-open-software-for-hevc-video-calls)

`J. Räsänen, M. Viitanen, J. Vanne, and T. D. Hämäläinen, “Kvazzup: open software for HEVC video calls,” in Proc. IEEE Int. Symp. Multimedia, Taichung, Taiwan, Dec. 2017. `

[Live Demonstration: Kvazzup 4K HEVC Video Call](https://researchportal.tuni.fi/en/publications/live-demonstration-kvazzup-4k-hevc-video-call)

`J. Räsänen, M. Viitanen, J. Vanne, and T. D. Hämäläinen, “Live Demonstration: Kvazzup 4K HEVC Video Call,” in Proc. IEEE Int. Symp. Multimedia, Taichung, Taiwan, Dec. 2018. `

[Live Demonstration : Interactive Quality of Experience Evaluation in Kvazzup Video Call](https://researchportal.tuni.fi/en/publications/live-demonstration-interactive-quality-of-experience-evaluation-i)

`J. Räsänen, A. Altonen, A. Mercat, and J. Vanne, “Live Demonstration : Interactive Quality of Experience Evaluation in Kvazzup Video Call,” in Proc. IEEE Int. Symp. Multimedia, Naples, Italy, Dec. 2020. `

## Planned features

- Media server integration
- TLS Encryption for SIP
- Contact presence monitoring
