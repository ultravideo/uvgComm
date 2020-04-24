Kvazzup
=======

Kvazzup is an HEVC video call software written in C++ and built on [Qt](https://www.qt.io/) framework. Kvazzup makes use of the following open-source tools: Kvazaar for HEVC encoding, OpenHEVC for HEVC decoding, Opus Codec for audio encoding and decoding, kvzRTP for Streaming Media and Speex DSP for AEC. The aim of this project is to become a state-of-the-art Open-source Video Conferencing application as well as to provide a testbed for novel video call technologies that improve the video call experience.

## Features 

Kvazzup has the following features:  
- SIP server integration for iniating a call with anyone who has registered to the server.
- Peer-to-peer call intiation (firewall needs to have an open port 5060 for incoming TCP).
- Peer-to-peer media transport through firewall with ICE.
- Contact list.
- Enable/disable audio or video.  
- Screen sharing.
- Live call parameter adjustment. 
- A statistics window for monitoring the call.
- Settings which are saved to disk.


## Compile Kvazzup

Kvazzup requires the following external libraries to run:  
- [Kvazaar](https://github.com/ultravideo/kvazaar) for video encoding.
- [OpenHEVC](https://github.com/OpenHEVC/openHEVC) for video decoding.
- [Opus](http://opus-codec.org/) for audio encoding and decoding.
- [kvzRTP](https://github.com/ultravideo/kvzRTP) for Media Delivery.
- [Speex DSP](https://www.speex.org/) for AEC.

Build these libraries using GCC, MinGW or Visual Studio. Make sure you use the same compiler for all dependencies and Kvazzup. OpenMP also need to be installed in your build environment or PATH. Qt Creator is recommended tool for compiling Kvazzup.

Few notes:
- Due to recent addition of kvzRTP, the Visual Studio compilation does not succeed. Fix is pending.
- The Linux version has a bug with QCamera which prevents choosing the resolution and the Opus codec is disabled until issues with it can be resolved.
- The current master still has few small bugs due to recent completion of several key features. A bug-free release is currently in the works and is expected to be ready soon. 


## Planned features

- Authentication (passwords)
- Contact presence monitoring
- Video conferences
- Encryption
- Automatic call parameter adjustment (currently everything is manual)
