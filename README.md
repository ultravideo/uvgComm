Kvazzup
=======

Kvazzup is a *High Efficiency Video Coding (HEVC)* video call software written in C++ and built on [Qt](https://www.qt.io/) application framework. The aim of Kvazzup is to be pave way for better quality video calls while valuing usability, security and privacy. Kvazzup makes use of the following open-source tools: Kvazaar for HEVC encoding, OpenHEVC for HEVC decoding, Opus Codec for audio encoding and decoding, uvgRTP for media delivery and Speex DSP for *Acoustic Echo Cancellation (AEC)*. 

Kvazzup is under development and new features will become available.

## Current features 

Currently Kvazzup has the following features:
- Initiating call through *Session Initiation Protocol (SIP)* proxy with authentication
- Alternatively, initiating call peer-to-peer (firewall needs to have incoming TCP port 5060 open)
- Peer-to-peer media delivery with NAT traversal using *Interactive Connectivity Protocol (ICE)*
- Media delivery encryption using *Secure Real-time Transport Protocol (SRTP)* and *Zimmermann RTP (ZRTP)*
- Contacts list
- Enable/disable audio and video, including at the other end with Re-INVITE
- Screen sharing
- Media settings which are saved to the disk
- Automatic selection of best media settings with option for live manual adjustment
- A statistics window for monitoring call quality

## Used Request for Comments (RFC) specifications

**Signaling**
- [RFC 3261 SIP: Session Initiation Protocol](https://www.rfc-editor.org/rfc/rfc3261)
- [RFC 5626 Managing Client-Initiated Connections in the Session Initiation Protocol (SIP)](https://www.rfc-editor.org/rfc/rfc5626)
- [RFC 5627 Obtaining and Using Globally Routable User Agent URIs (GRUUs) in the Session Initiation Protocol (SIP)](https://www.rfc-editor.org/rfc/rfc5627)
- [RFC 5923 Connection Reuse in the Session Initiation Protocol (SIP)](https://www.rfc-editor.org/rfc/rfc5923)
- [RFC 2617 HTTP Authentication: Basic and Digest Access Authentication](https://www.rfc-editor.org/rfc/rfc2617)

**Negotiation**
- [RFC 8866 SDP: Session Description Protocol](https://datatracker.ietf.org/doc/html/rfc8866)
- [RFC 3264 An Offer/Answer Model with the Session Description Protocol (SDP)](https://www.rfc-editor.org/rfc/rfc3264.html)
- [RFC 3551 RTP Profile for Audio and Video Conferences with Minimal Control](https://www.rfc-editor.org/rfc/rfc3551)

**Connectivity**
- [RFC 8445 Interactive Connectivity Establishment (ICE): A Protocol for Network Address Translator (NAT) Traversal](https://www.rfc-editor.org/rfc/rfc8445)
- [RFC 5389 Session Traversal Utilities for NAT (STUN)](https://www.rfc-editor.org/rfc/rfc5389)

**Delivery**

See [uvgRTP](https://github.com/ultravideo/uvgRTP).

## Supported Camera Input Formats

- YUV 420 P (also known as I420)
- YUV 422 P (also known as I422)
- NV 12
- NV 21
- YUYV (also known as YUY2)
- UYVY
- ARGB
- BGRX (also known as BGR24)
- BGRA (also known as BGR32)
- ABGR
- RGBA (also known as RGB32)
- RGBX (also known as RGB24)
- Motion JPEG

## Compile Kvazzup

Kvazzup requires the following external libraries to operate: 
- [Kvazaar](https://github.com/ultravideo/kvazaar) for HEVC encoding
- [OpenHEVC](https://github.com/OpenHEVC/openHEVC) for HEVC decoding
- [libyuv](https://chromium.googlesource.com/libyuv/libyuv/) for video input processing
- [Opus](http://opus-codec.org/) for audio coding
- [Speex DSP](https://www.speex.org/) for audio processing
- [uvgRTP](https://github.com/ultravideo/uvgRTP) for media delivery
- [Crypto++](https://cryptopp.com/) for delivery encryption (optional)

Kvazzup uses CMake to build itself and missing dependencies with minimal user effort, see [BUILDING.md](BUILDING.md) for build instructions.

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
