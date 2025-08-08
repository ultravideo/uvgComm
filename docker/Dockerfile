FROM ubuntu:22.04

# Set non-interactive frontend for automation
ENV DEBIAN_FRONTEND=noninteractive

# Install required build/runtime dependencies plus OpenGL libs and Qt OpenGL plugin
RUN apt-get update && apt-get install -y \
  cmake \
  qt6-base-dev \
  libqt6svg6-dev \
  qt6-multimedia-dev \
  libspeexdsp-dev \
  libomp-dev \
  libgl1 \
  libxcb-xinerama0 \
  libxcb-xinerama0-dev \
  libxkbcommon-x11-0 \
  libgl1-mesa-dev \
  libopengl0 \
  libcrypto++-dev \
  libopus-dev \
  yasm \
  libjpeg-dev \
  xvfb \
  git \
  build-essential \
  libx11-6 libxext6 libxrender1 libxfixes3 \
  && apt-get clean

# Create working directory
WORKDIR /uvgcomm

# Copy source tree
COPY . .

# Build project (you can adjust build options here)
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build -j$(nproc) \
 && ls -l build

# Set default working dir for runtime
WORKDIR /uvgcomm/build

# Copy the run script
COPY docker/run.sh /run.sh
RUN chmod +x /run.sh

ENTRYPOINT ["/run.sh"]
