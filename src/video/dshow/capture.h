#pragma once
#include <Windows.h>
#include <dshow.h>
#include <cstdlib>
#include <iostream>
//#include <atlbase.h>
#include <initguid.h>
#include <cstdint>
#include <deque>
#include <vector>
#include "SampleGrabber.h"
#include "capture_interface.h"

struct frameData {
  uint8_t* data;
  int32_t datalen;
};

class DShow_Capture {

public:

  int8_t selectedDevice;
  int8_t selectedCapability;

  DShow_Capture() : selectedDevice(0) {

  }
   

  int8_t init();

  int8_t cleanup();
  int8_t queryDevices();
  int8_t queryCapabilities();
  int8_t setCapability(IBaseFilter* filter, int capability);
  int8_t pushFrame(uint8_t *data, int32_t datalen);

  int8_t startCapture();
  int8_t play();
  int8_t stop();

  int32_t getFrameCount() { return frames.size(); }

  int8_t getFrame(uint8_t** data, uint32_t *size);

  int8_t selectDevice(int devId);
  IBaseFilter* CreateFilterFromDevice(int devId);
  IPin* getPin(IBaseFilter *pFilter, LPCOLESTR pinname);

  std::vector<std::string> devices;
  std::vector<deviceCapability> device_capabilities;

  bool rgb32_;

private:

  std::deque<frameData> frames;  

  IGraphBuilder *pGraph         = NULL;
  ICaptureGraphBuilder2 *pBuild = NULL;
  ICreateDevEnum *pDevEnum      = NULL;
  IEnumMoniker *pEnum           = NULL;
  IMoniker* pMoniker            = NULL;
  IBaseFilter *pCap             = NULL;
  IMediaControl *pControl       = NULL;
  IMediaEvent   *pEvent         = NULL;


  // Filters
  IBaseFilter* inputFilter      = NULL;
  IBaseFilter *pNullF           = NULL;
  IBaseFilter *pGrabberF        = NULL;
  ISampleGrabber *pGrabber      = NULL;
};
