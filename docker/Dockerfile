FROM ubuntu:22.04

# Set non-interactive frontend for automation
ENV DEBIAN_FRONTEND=noninteractive

# Install required build/runtime dependencies (OpenGL is needed by Qt)
RUN apt-get update && apt-get install -y \
  git \
  cmake \  
  build-essential \ 
  libgl1 \ 
  libgl1-mesa-dev \ 
  libopengl0 \   
  qt6-base-dev \   
  libqt6svg6-dev \ 
  qt6-multimedia-dev \  
  libspeexdsp-dev \
  libopus-dev \  
  libomp-dev \  
  libcrypto++-dev \ 
  yasm \    
  xvfb \        
  && apt-get clean

# optional packages for camera support
RUN apt-get install -y \
  libjpeg-dev \
  gstreamer1.0-plugins-base \
  gstreamer1.0-plugins-good \
  && apt-get clean

# optional packages for x-forwarding from the docker container
RUN apt-get install -y \
  libxcb-xinerama0 \
  libxcb-xinerama0-dev \
  libxkbcommon-x11-0 \
  libx11-6 libxext6 libxrender1 libxfixes3 \
  && apt-get clean

# Create working directory
WORKDIR /uvgcomm

# Copy source tree
COPY . .

# Build project (you can adjust build options here)
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build -j$(nproc)

# Set default working dir for runtime
WORKDIR /uvgcomm/build

# Copy the run script
COPY docker/run.sh /run.sh
RUN chmod +x /run.sh

ENTRYPOINT ["/run.sh"]
