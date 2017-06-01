#pragma once
#include <cstdint>

struct deviceCapability {
  int width;
  int height;
  double fps;
  char format[5]; // Null-terminated string
  char reserved[95];
};


extern "C" int8_t dshow_initCapture();
extern "C" int8_t dshow_queryDevices(char **devices[], int8_t *count);
extern "C" int8_t dshow_selectDevice(int device);
extern "C" int8_t dshow_getDeviceCapabilities(deviceCapability** list, int8_t *count);
extern "C" int8_t dshow_setDeviceCapability(int capability);
extern "C" int8_t dshow_startCapture();
extern "C" int8_t dshow_play();
extern "C" int8_t dshow_stop();
extern "C" int8_t dshow_queryFrame(uint8_t** data, uint32_t *size);
extern "C" int32_t dshow_getFrameCount();
