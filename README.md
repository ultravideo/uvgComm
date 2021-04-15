Kvazzup
=======

Kvazzup is a *High Efficiency Video Coding (HEVC)* video call software written in C++ and built on [Qt](https://www.qt.io/) application framework. The aim of Kvazzup is to be pave way for better quality video calls while valuing usability, security and privacy. Kvazzup is also designed to work as a testbed novel video call technologies that improve the video call experience. Kvazzup makes use of the following open-source tools: Kvazaar for HEVC encoding, OpenHEVC for HEVC decoding, Opus Codec for audio encoding and decoding, uvgRTP for media delivery and Speex DSP for *Acoustic Echo Cancellation (AEC)*. 

Kvazzup is under development and new features will become available.

## Current features 

Currently Kvazzup has the following features:
- Initiating call through *Session Initiation Protocol (SIP)* proxy (authentication is not yet supported)
- Initiating call peer-to-peer (firewall needs to have an open port 5060 for incoming TCP)
- Peer-to-peer media delivery with NAT traversal using *Interactive Connectivity Protocol (ICE)*
- Contacts list
- Enable/disable audio and video
- Screen sharing
- Video and audio Settings which are saved to the disk
- Live call parameter adjustment
- A statistics window for monitoring the call quality

## Compile Kvazzup

Kvazzup requires the following external libraries to operate: 
- [Kvazaar](https://github.com/ultravideo/kvazaar) for HEVC encoding
- [OpenHEVC](https://github.com/OpenHEVC/openHEVC) for HEVC decoding
- [Opus](http://opus-codec.org/) for audio coding
- [uvgRTP](https://github.com/ultravideo/uvgRTP) for media delivery
- [Speex DSP](https://www.speex.org/) for audio processing
- [Crypto++](https://cryptopp.com/) for delivery encryption

Qt Creator is the recommended tool for compiling Kvazzup. Make sure you use the same compiler and bit version for all the dependencies and for Kvazzup. If you want to use media encryption, you should compile uvgRTP with crypto++ by adding `-D__RTP_CRYPTO__` to uvgRTP CXXFLAGS.

### Linux(GCC)

Install Qt and Qt multimedia. Make sure Opus, Speex DSP, OpenMP and Crypto++ are installed. Compile and install openHEVC, Kvazaar and uvgRTP.

### MinGW

Make sure OpenMP is installed in your build environment. Add compiled libraries to PATH or to `../libs` folder and headers to PATH or `../include`.

### Microsoft Visual Studio

Compile the dependencies. Add compiled libraries to PATH or to `../msvc_libs` folder, and headers to PATH or `../include`. 

#### Shared Kvazaar

When compiling Kvazaar, select Dynamic Lib(.dll) in kvazaar_lib project Properties: General/Configuration Type and add ;PIC to C/C++ Preprocessor/Preprocessor Definitions. 

In Kvazzup, please make sure:`DEFINES += PIC` is included in Kvazzup.pro file. 

#### Static Kvazaar

Please uncomment: `DEFINES += KVZ_STATIC_LIB` in Kvazzup.pro file.

## Paper

If you are using Kvazzup in your research, please refer to the following [paper](https://ieeexplore.ieee.org/abstract/document/8241673): <br>
`J. Räsänen, M. Viitanen, J. Vanne, and T. D. Hämäläinen, “Kvazzup: open software for HEVC video calls,” in Proc. IEEE Int. Symp. Multimedia, Taichung, Taiwan, Dec. 2017. `


## Known issues

- The Linux version of Kvazzup has a bug with QCamera which prevents from changing the default resolution. 
- The Opus codec is disabled on Linux until issues with it have been resolved.

## Planned features

- RFC 3261 Authentication
- Contact presence monitoring
- Multiparty video conferences
- TLS Encryption
- Automatic call parameter adjustment
