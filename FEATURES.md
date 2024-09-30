uvgComm Features
================

The features list for uvgComm is constantly expanding and here is the detailed description of current features in uvgComm.



## uvgComm Protocols

### Session Initiation Protocol (SIP)

SIP is a signaling protocol that enables uvgComm to start, answer or end calls. SIP uses a specific set of messages to initiate the call. SIP messages have a header and a payload section.

uvgComm has been tested to work with [Kamailio](https://www.kamailio.org/) SIP proxy. Proxy is what enables uvgComm to receive calls from anyone on the internet and makes uvgComm work similarly to any commercial communication software. Using a proxy with uvgComm requires setting up the proxy and creating necessary user accounts. After this, the username and the proxy address can be used together to both register (by sending a REGISTER-message) to the proxy and to call other users via the proxy.

uvgComm can also be used to directly call other computers provided that the TCP connection can be formed. Usually this involves allowing incoming TCP connections on port 5060 on the firewall. Direct calling is mostly meant for testing purposes and any serious usage should consider setting up a SIP proxy.

**Specifications**
- [RFC 3261 SIP: Session Initiation Protocol](https://www.rfc-editor.org/rfc/rfc3261)
- [RFC 5626 Managing Client-Initiated Connections in the Session Initiation Protocol (SIP)](https://www.rfc-editor.org/rfc/rfc5626)
- [RFC 5627 Obtaining and Using Globally Routable User Agent URIs (GRUUs) in the Session Initiation Protocol (SIP)](https://www.rfc-editor.org/rfc/rfc5627)
- [RFC 5923 Connection Reuse in the Session Initiation Protocol (SIP)](https://www.rfc-editor.org/rfc/rfc5923)
- [RFC 2617 HTTP Authentication: Basic and Digest Access Authentication](https://www.rfc-editor.org/rfc/rfc2617)

### Session Description Protocol (SDP)

Some SIP messages carry an SDP messages as a payload during call initiation. SDP is used for describing the media parameters wanted for the call and is used to negotiatiate various aspects of the call.

uvgComm has SDP implements forming of a P2P Mesh based conference. Here each participants *synchronization sources (SSRC)* are negotiated as part of the call negotiations.

**Specifications**
- [RFC 8866 SDP: Session Description Protocol](https://datatracker.ietf.org/doc/html/rfc8866)
- [RFC 3264 An Offer/Answer Model with the Session Description Protocol (SDP)](https://www.rfc-editor.org/rfc/rfc3264.html)
- [RFC 5576 Source-Specific Media Attributes in SDP](https://www.rfc-editor.org/rfc/rfc5576.html)
- [RFC 5888 The SDP Grouping Framework](https://www.rfc-editor.org/rfc/rfc5888.html)

### Interactive Connectivity Establishement (ICE)

When the start of the call has been agreed upon, ICE protocol starts testing various candidates to find the optimal route for the media traversal. ICE candidates are transferred inside the SDP message.

**Specifications**
- [RFC 8445 Interactive Connectivity Establishment (ICE): A Protocol for Network Address Translator (NAT) Traversal](https://www.rfc-editor.org/rfc/rfc8445)
- [RFC 5389 Session Traversal Utilities for NAT (STUN)](https://www.rfc-editor.org/rfc/rfc5389)

### Real-Time Transport Protocol (RTP)

After the best path for media has been found, uvgComm sets up the delivery using [uvgRTP](https://github.com/ultravideo/uvgRTP). uvgRTP handles the fragmentation of video frames so they go smoothly through the network.

**Specifications**
See [uvgRTP](https://github.com/ultravideo/uvgRTP) for more details.




## Media 

For media processing, uvgComm has implemented a filter graph where each filter modifies to media data in a desirable way.

### HEVC codec for video

uvgComm uses [Kvazaar](https://github.com/ultravideo/kvazaar) HEVC encoder to encode the video, uvgRTP to deliver it and [OpenHEVC](https://github.com/OpenHEVC/openHEVC) HEVC decoder to decode it.

### Opus codec for audio

Opus support is provided by the [Opus](http://opus-codec.org/) codec library.

### Support for 13 different camera input pixel formats

Here is the current list of supported input formats:

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

The support must first be provided by Qt and then uvgComm uses [libyuv](https://chromium.googlesource.com/libyuv/libyuv/) to convert the format into YUV 420 P that is used by the HEVC encoder Kvazaar. The settings manage the input selection by excluding formats that are not supported from both automatic and user selections.

### Option to disable audio and/or video

Coming soon ...

### Screen sharing

uvgComm uses Qt to provide ability for screen capture by taking screen shots at constant rate.




## Analytics and Settings

### A statistics window for monitoring call quality

The statistics window of uvgComm shows 1) the SIP log, 2) the call parameters, 3) statistics for delivery, 4) Filter Graph buffer statuses, and 5) the call performance. The  statistics window can be used to verify correct operations and monitor the performance characteristics of uvgComm.

### Full customizability of both audio and video processing in settings

Since uvgComm uses the codecs directly, uvgComm can also offer full control for the codecs directly to user. User can adjust almost any codec parameter to their liking from settings User Interface.

In addition to codec settings, the settings enable changing other aspects of audio and video processing, such as disabling muting audio with incoming audio to avoid audio echo.

### Automatic selection of best media settings with option for live manual adjustment

To not require user to be fully knowledgeable in codec parameters, uvgComm also tries to guess the optimal parameters for currently used computer. This process is subject to improvements.

### All settings are recorded to the disk and loaded at startup

uvgComm records the settings to the disk using Qts QSettings class. At startup the settings are loaded both for the software and for the software settings UI.




## Other

### Contacts List

uvgComm has contact lists where user can add contacts and call them. The contact lists are preserved even if uvgComm is shut down.