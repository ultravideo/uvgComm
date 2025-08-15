# Running uvgComm in Docker

This folder contains files needed to run *uvgComm* in a Docker container. The Dockerfile contains the build details and 'run.sh' is the default script.

## Prerequisites

First, install Docker:

```bash
apt install docker.io
```

Add rights to user docker to your user:

```bash
usermod -aG docker <user>
```

Remember to log out if you do this for the current user.

## Building the Docker Image

In this folder ('/docker'), run:

```bash
docker build --no-cache -t uvgcomm-docker -f Dockerfile ..
```

## Running the Docker Image

Run *uvgComm* with modified script and configuration:

```bash
docker run --rm -it \
  -v <path to script>:/run.sh:ro \
  -v <path to ini configuration>:/uvgcomm/build/uvgComm.ini \
  --net=host \
  uvgcomm-docker /run.sh
```

Best way to create an *uvgComm* ini configuration file is to run *uvgComm*, modify settings, save and copy the settings file.

## Run uvgComm with camera

An udev service is needed for Qt to detect camera devices. *Docker* does not have running udev service so you either have to start one or share the hosts udev folder:

```bash
docker run --rm -it --device /dev/<videoX> -v /run/udev:/run/udev:ro uvgcomm-docker /run.sh
```

This enabled uvgComm to see video devices mounted to container with ´--device´-flag.

## Creating a virtual camera

In case you are performing experimental evaluations and want to simulate a web cam, you could do something like this:

```bash
apt install ffmpeg
apt install v4l2loopback-dkms v4l-utils
modprobe v4l2loopback video_nr=<X> card_label="FakeCam" exclusive_caps=1
usermod -aG video <user>
```

Then logout so change takes effect. The video can be streamed with following command:

```bash
ffmpeg -re -stream_loop -1 -framerate <framerate> -s <resolution> -i <filename> -f v4l2 /dev/<videoX>
```

Then you can use the created device as it was an actual camera.

We found that calling v4l2 utils inside the container breaks the video device which just gives 'ioctl(VIDIOC_G_FMT): Invalid argument' error. If this you may you need to remove it:

```bash
modprobe -r v4l2loopback
```

And then recreate it.

## View Container uvgComm locally

If you have linux host, you can just access the x server from docker container directly:
```bash
xhost +local:docker
docker run -it \
    -e DISPLAY=$DISPLAY \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    uvgcomm-docker /run.sh
```

## Run uvgComm over X-forwarding

If you need to control the uvgComm insde the container from another machine than the host, you can use x-forwarding:

```bash
docker run --rm -it --env DISPLAY=$DISPLAY   --volume /tmp/.X11-unix:/tmp/.X11-unix --volume="$HOME/.Xauthority:/root/.Xauthority:rw" --net=host uvgcomm-docker /run.sh
```

This makes container use the host networking, which is not always desirable.
