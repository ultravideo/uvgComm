Kvazzup
=======

Kvazzup is an HEVC video call software written in C++ and built on [Qt](https://www.qt.io/) framework. Kvazzup makes use of the following open-source tools: Kvazaar for HEVC encoding, OpenHEVC for HEVC decoding, Opus Codec for audio encoding and decoding, uvgRTP for Streaming Media and Speex DSP for AEC. The aim of this project is to become a state-of-the-art Open-source Video Conferencing application as well as to provide a testbed for novel video call technologies that improve the video call experience.

Kvazzup is still under development and new features will become available. Current version has few small bugs due to recent completion of several key features. A bug-free release is currently in the works and is expected to be ready soon.

## Current features 

Currently Kvazzup has the following features:
- SIP server call initiation with anyone who has registered to the server.
- Peer-to-peer call intiation (firewall needs to have an open port 5060 for incoming TCP).
- Peer-to-peer media transport through firewall with ICE.
- Contact list.
- Enable/disable audio or video.
- Screen sharing.
- Live call parameter adjustment. 
- A statistics window for monitoring the call.
- Video and Audio Settings which are saved to the disk.


## Compile Kvazzup

Kvazzup requires the following external libraries to run: 
- [Kvazaar](https://github.com/ultravideo/kvazaar) for video encoding.
- [OpenHEVC](https://github.com/OpenHEVC/openHEVC) for video decoding.
- [Opus](http://opus-codec.org/) for audio encoding and decoding.
- [uvgRTP](https://github.com/ultravideo/uvgRTP) for media delivery.
- [Speex DSP](https://www.speex.org/) for AEC.
- [Crypto++](https://www.cryptopp.com/) for media encryption.

Qt Creator is the recommended tool for compiling Kvazzup. Make sure you use the same compiler and bit version for all the dependencies and for Kvazzup. You should compile uvgRTP with crypto++ by adding `-D__RTP_CRYPTO__` to uvgRTP CXXFLAGS.

### Linux(GCC)

Install Qt and Qt multimedia. Make sure Opus, Speex DSP and OpenMP are installed. Compile and install openHEVC, Kvazaar and uvgRTP.

### MinGW

Make sure OpenMP is installed in your build environment. Add compiled libraries to PATH or to `../libs` folder and headers to PATH or `../include`.

### Microsoft Visual Studio

Compile the dependencies. If you get the following error `Windows SDK version X.X was not found`, retarget the solution.

Add compiled libraries to PATH or to `../msvc_libs` folder and headers to PATH or `../include`. 

#### Shared Kvazaar

When compiling Kvazaar, select Dynamic Lib(.dll) in kvazaar_lib project Properties: General/Configuration Type and add ;PIC to C/C++ Preprocessor/Preprocessor Definitions. 

In Kvazzup, please add:`DEFINES += PIC` to Kvazzup.pro file. 

#### Static Kvazaar

Please add: `DEFINES += KVZ_STATIC_LIB` to Kvazzup.pro file.

## Known issues

- The Linux version of Kvazzup has a bug with QCamera which prevents from changing the default resolution. 
- The Opus codec is disabled on Linux until issues with it have been resolved.


## Planned features

- Authentication (passwords)
- Contact presence monitoring
- Video conferences
- Encryption
- Automatic call parameter adjustment (currently everything is manual)
