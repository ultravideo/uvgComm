Kvazzup
=======

Kvazzup is a *High Efficiency Video Coding (HEVC)* video call software written in C++ and built on [Qt](https://www.qt.io/) application framework. The aim of Kvazzup is to be pave way for better quality video calls while valuing usability, security and privacy. Kvazzup makes use of the following open-source tools: Kvazaar for HEVC encoding, OpenHEVC for HEVC decoding, Opus Codec for audio encoding and decoding, uvgRTP for media delivery and Speex DSP for *Acoustic Echo Cancellation (AEC)*. 

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
- Media delivery encryption

## Compile Kvazzup

Kvazzup requires the following external libraries to operate: 
- [Kvazaar](https://github.com/ultravideo/kvazaar) for HEVC encoding
- [OpenHEVC](https://github.com/OpenHEVC/openHEVC) for HEVC decoding
- [Opus](http://opus-codec.org/) for audio coding
- [uvgRTP](https://github.com/ultravideo/uvgRTP) for media delivery
- [Speex DSP](https://www.speex.org/) for audio processing
- [Crypto++](https://cryptopp.com/) for delivery encryption

Qt Creator is the recommended tool for compiling Kvazzup. Make sure you use the same compiler and bit version for all the dependencies and for Kvazzup. It is possible, although not recommended to use Kvazzup without Crypto++.

Kvazzup uses code indent of 2. You can change that in qt creator from Tools -> Options -> C++ -> Code Style by making a new code style with indent and tab sizes of 2.

### Linux(GCC)

Install Qt, Qt multimedia and QtCreator. Make sure Opus, Speex DSP, OpenMP and Crypto++ are installed. Compile and install [OpenHEVC](https://github.com/OpenHEVC/openHEVC), [Kvazaar](https://github.com/ultravideo/kvazaar) and [uvgRTP](https://github.com/ultravideo/uvgRTP).

**Ubuntu**

On Ubuntu, the necessary Qt packets are `qt5-default qtdeclarative5-dev libqt5svg5-dev qtmultimedia5-dev libqt5multimedia5-plugins libqt5multimediawidgets5`.

Next, install `qtcreator`.

The following Kvazzup dependencies are available as packages: `libopus-dev libspeexdsp-dev libomp-dev libcrypto++-dev`. For the OpenHEVC, Kvazaar and uvgRTP, if they are not available, you will have to clone the Github repositories and compile them according to their respective instructions.

If you have trouble with Qt creator code highlights, but not compilation, make sure you have the correct version of Clang installed. When testing, the Ubuntu had clang-10 installed when Qt depended on clang-8. Installing `clang-8` may solve this issue.

Note: We have not been able to get the changing of the camera formats, resolutions or frame rates to work on Linux. It is possible that we didn't have all the necessary packets installed or that there is some sort of bug in qt multimedia/multimediawidgets on Ubuntu. Any information on this would be greatly appreciated.

### MinGW

Make sure OpenMP is installed in your build environment. Add compiled libraries to PATH or to `../libs` folder and headers to PATH or `../include`.

### Microsoft Visual Studio

Compile the dependencies. Add compiled libraries to PATH or to `../msvc_libs` folder, and headers to PATH or `../include`. If you don't exclude Crypto++, you need to compile it with dynamic C++ runtime linking. See [this section](https://cryptopp.com/wiki/Visual_Studio#Runtime_Linking) for detailed instructions. After that, the easiest way to compile is to select `Build -> Batch Build...`. You must also make sure all static libraries (usually uvgRTP and Crypto++) have been compiled with same debug level (debug/release) as Kvazzup.

**Shared Kvazaar**

When compiling Kvazaar, select Dynamic Lib(.dll) in kvazaar_lib project Properties: General/Configuration Type and add `;PIC` to C/C++ Preprocessor/Preprocessor Definitions. 

In Kvazzup, please make sure: `DEFINES += PIC` is included in Kvazzup.pro file. 

**Static Kvazaar**

Please uncomment: `DEFINES += KVZ_STATIC_LIB` in Kvazzup.pro file.

## Paper

If you are using Kvazzup in your research, please refer to the following [paper](https://ieeexplore.ieee.org/abstract/document/8241673): <br>
`J. Räsänen, M. Viitanen, J. Vanne, and T. D. Hämäläinen, “Kvazzup: open software for HEVC video calls,” in Proc. IEEE Int. Symp. Multimedia, Taichung, Taiwan, Dec. 2017. `


## Planned features

- RFC 3261 Authentication
- Contact presence monitoring
- Multiparty video conferences
- TLS Encryption
- Automatic call parameter adjustment
