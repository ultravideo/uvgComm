## Building Kvazzup

Kvazzup is a Qt 6 application which has several libraries as dependencies and uses CMake as its build system. To make building easier, the Cmake system has been set up in such a way that it automatically downloads the dependencies which are not easily available on the system, making building Kvazzup simple both on Linux with GCC and Windows with Microsoft Visual Studio. 

The primary methods for installing dependencies are 1) preinstall by user (Linux), 2) building from source (Linux and Windows) and 3) downloading ready binaries for use(Windows). The following table lists the current status of methods used for acquiring dependency on each platform.

For preinstalled packets, Kvazzup uses both find_package and pkgconfig to try to find packets. If either of these finds the packet, it is considered installed and will not be downloaded.

This tutorial assumes that `git` is installed and the Kvazzup repository has been cloned. 

### Linux with GCC

#### Install dependencies on Linux

**Required packages**

On linux you must install the following packages to build kvazzup: 

```
apt install cmake qt6-base-dev libqt6svg6-dev qt6-multimedia-dev libspeexdsp-dev libomp-dev
```

**Recommended packages**
It is also recommended to install Crypto++ and opus as preinstall reduces compilation time and disk usage:

```
apt install libcrypto++-dev libopus-dev
```

If uvgRTP, Kvazaar or OpenHEVC packages are available, you are welcome to install those as well to reduce compilation time. Installing libyuv-dev may not help, since the package did not seem to provide the necessary files for CMake to find it even if it is installed.

**Optional packages**
As for optional dependencies, OpenHEVC has an optional dependency in Yasm, enabling more optimizations. If you are using Qt version 6.5 or higher, you should install libjpeg in case your webcamera is using motion jpeg. Qt versions before 6.5 provide motion jpeg as 32-bit RGB which is supported without libjpeg.

```
apt install yasm libjpeg-dev
```

#### Building on Linux

**Option 1: Qt Creator**

First, install Qt Creator :
```
apt install qtcreator
```

On Linux Qt Creator should work if necessary dependencies are installed. Just open the `CMakeList.txt` file using Qt Creator. In my testing the release version builds well on Linux by default, but for debug version I had to set CMAKE_BUILD_TYPE=Debug otherwise I got a warning about missing configuration.

**Option 2: Cmake**

On linux, Cmake works because Qt path has been set during installation. This means you can just use the normal method to install Kvazzup with CMake:

```
mkdir build
cd build
cmake ..
make
```

Calling `make install` also seems to work, but is not recommended until the installation of dependencies has been sorted out.

### Windows with Visual Studio

This part of instructions assumes that Microsoft Visual Studio 2019 or newer has been installed.

#### Dependencies

You will need a `Qt Account` to download Qt. Download the open-source `Qt Online Installer` from https://www.qt.io/download-open-source. You may also use the commercial version if available. 

Select Custom install and then install the following components:
```
Qt -> Qt <Release number> -> MSVC 64 bit
Qt -> Qt <Release number> -> Additional Libraries -> Qt Multimedia
```

If you wish to make changes to Kvazzup, you may also want the following components:
```
Qt -> Qt <Release number> -> Sources
Qt -> Qt <Release number> -> Qt Debug Information Files
```

If you want to use Qt Creator for building Kvazzup (Recommended), you must also make sure that following components are selected:
```
Qt -> Developer and Designer Tools -> Qt Creator <version>
Qt -> Developer and Designer Tools -> Qt Creator <version> CDB Debugger Support
Qt -> Developer and Designer Tools -> Debugging Tools for Windows
Qt -> Developer and Designer Tools -> Cmake <version>
Qt -> Developer and Designer Tools -> Ninja <version>
```

Rest of the dependencies are downloaded automatically by Kvazzup CMake. You can also try installing various libraries and having them be available via PATH variable, but this method is outside the scope of these instructions.

#### Building on Windows

**Option 1: Qt Creator**

Open the CMakeLists.txt file with Qt Creator and wait for the CMake to run (output seen in the `General Messages`-tab). After CMake has finished, you can build Kvazzup. It will take a while to compile all the dependencies, and you may check the progress in `Compiler Output`-tab. Sometimes the build may result in error if the CMake is run at the same time as build, but rerunning the build should solve the issue.

To deploy the built Kvazzup as a binary, you only need to modify the `Projects -> Build Settings -> Build Steps` to include install. This will install Kvazzup to location indicated by the `CMAKE_INSTALL_PREFIX`-variable along with all necessary libraries and assets.

**Option 2: Cmake**

Using CMake without Qt Creator should be possible on windows as well, but unlike on Linux, installing Qt does not automatically set QT_DIR which indicates the location of Qt files. In order to use CMake alone on Windows, you must set QT_DIR to correct location. Then you can use the normal CMake build process to build Kvazzup (either `cmake-gui` to build using a GUI or `Git Bash` to run same commands as on Linux).