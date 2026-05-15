uvgComm
=======

uvgComm is a *Real‑Time Communication (RTC)* video call software written in C++ and built on the [Qt](https://www.qt.io/) application framework. It is an experimental application that supports a hybrid *Peer‑to‑peer (P2P)* mesh and *Selective Forwarding Unit (SFU)* architecture with real‑time network latency measurement-based route-optimization. Together with the `uvgcomm-benchmark` repository, it can also serve as a reproducible research testbed for evaluating RTC research. uvgComm is licensed under the permissive ISC-license. uvgComm was previously called Kvazzup. 

## Current features 

**Supported Architectures**
- Plain one-to-one calls
- P2P mesh
- SFU
- Hybrid between P2P Mesh and SFU

**Protocols**
- Signaling with *Session Initiation Protocol (SIP)*
- Negotiation with *Session Description Protocol (SDP)*
- NAT traversal with *Session Traversal Utilities for NAT (STUN)*
- Delivery with *Real-Time Transport Protocol (RTP)*

**Media**
- *High Efficiency Video Coding (HEVC)* codec for video
- Opus codec for audio
- Support for 13 different camera input pixel formats

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

uvgComm uses CMake to build itself and missing dependencies with minimal effort for the developer, see [BUILDING.md](BUILDING.md) for build instructions.

## Setting up a SIP Server

uvgComm has been tested together with *Kamailio Open Source SIP Server*. Easy to follow instructions are hard to come by on the internet, so we provide our own [instructions](kamailio/README.md) for setting up Kamailio. uvgComm can also call other uvgComm instances without a SIP proxy, but then firewall needs to be open.

## Instructions

You can find instructions on how to use uvgComm [here](INSTRUCTIONS.md).

## Benchmarking and Research

For automated experiments and reproducible benchmarking of end-to-end latency, packet loss, and media-quality metrics, see the `uvgComm-benchmark` repository: https://github.com/ultravideo/uvgComm-benchmark. The benchmark repository contains scripts, metric definitions, and instructions for running controlled evaluations with uvgComm.

## Papers

If you are using uvgComm in your research, please cite one of the following papers: <br>

This software was used in a manuscript submitted to *ACM Transactions on Multimedia Computing, Communications, and Applications (TOMM)*.

[uvgComm: Open Software for Low‑Latency Multi‑Party Video Communication](https://researchportal.tuni.fi/en/publications/uvgcomm-open-software-for-low-latency-multi-party-video-communica/)

`J. Räsänen, H. Tampio, A. Mercat, and J. Vanne, "uvgComm: Open Software for Low‑Latency Multi‑Party Video Communication."`

[Kvazzup: Open Software for HEVC Video Calls](https://researchportal.tuni.fi/en/publications/kvazzup-open-software-for-hevc-video-calls)

`J. Räsänen, M. Viitanen, J. Vanne, and T. D. Hämäläinen, "Kvazzup: Open Software for HEVC Video Calls," in Proc. IEEE Int. Symp. Multimedia, Taichung, Taiwan, Dec. 2017.`

[Live Demonstration: Kvazzup 4K HEVC Video Call](https://researchportal.tuni.fi/en/publications/live-demonstration-kvazzup-4k-hevc-video-call)

`J. Räsänen, M. Viitanen, J. Vanne, and T. D. Hämäläinen, "Live Demonstration: Kvazzup 4K HEVC Video Call," in Proc. IEEE Int. Symp. Multimedia, Taichung, Taiwan, Dec. 2018.`

[Live Demonstration: Interactive Quality of Experience Evaluation in Kvazzup Video Call](https://researchportal.tuni.fi/en/publications/live-demonstration-interactive-quality-of-experience-evaluation-i)

`J. Räsänen, A. Altonen, A. Mercat, and J. Vanne, "Live Demonstration: Interactive Quality of Experience Evaluation in Kvazzup Video Call," in Proc. IEEE Int. Symp. Multimedia, Naples, Italy, Dec. 2020.`

