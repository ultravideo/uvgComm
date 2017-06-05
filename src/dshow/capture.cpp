
#include <Windows.h>
#include <dshow.h>
#include <cstdlib>
#include <iostream>
//#include <atlbase.h>
#include <initguid.h>
#include "SampleGrabber.h"
#include "capture.h"

// mingw: g++ capture.cpp main_cli.cpp -lm -lstrmiids -lole32 -loleaut32
#pragma comment(lib,"Strmiids.lib") 

#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(ISampleGrabber, 0x6b652fff, 0x11fe, 0x4fce, 0x92, 0xad, 0x02, 0x66, 0xb5, 0xd7, 0xc7, 0x8f)
__CRT_UUID_DECL(ISampleGrabberCB, 0x0579154a, 0x2b53, 0x4994, 0xb0, 0xd0, 0xe7, 0x73, 0x14, 0x8e, 0xff, 0x85)
#endif

static DShow_Capture capture;

class CallbackObject : public ISampleGrabberCB {
public:

  CallbackObject() {};

  STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
  {
    if (NULL == ppv) return E_POINTER;
    if (riid == __uuidof(IUnknown)) {
      *ppv = static_cast<IUnknown*>(this);
      return S_OK;
    }
    if (riid == __uuidof(ISampleGrabberCB)) {
      *ppv = static_cast<ISampleGrabberCB*>(this);
      return S_OK;
    }
    return E_NOINTERFACE;
  }
  STDMETHODIMP_(ULONG) AddRef() { return S_OK; }
  STDMETHODIMP_(ULONG) Release() { return S_OK; }

  //ISampleGrabberCB
  STDMETHODIMP SampleCB(double SampleTime, IMediaSample *pSample);
  STDMETHODIMP BufferCB(double SampleTime, BYTE *pBuffer, long BufferLen) { std::cerr << "BufferCB" << std::endl; return S_OK; }
};

STDMETHODIMP CallbackObject::SampleCB(double SampleTime, IMediaSample *pSample)
{
  if (!pSample)
    return E_POINTER;
  long sz = pSample->GetActualDataLength();
  BYTE *pBuf = NULL;
  pSample->GetPointer(&pBuf);
  if (sz <= 0 || pBuf == NULL) return E_UNEXPECTED;
  capture.pushFrame((uint8_t*)pBuf, sz);
  pSample->Release();
  
  return S_OK;
}


DEFINE_GUID(CLSID_NullRenderer, 0xc1f400a4, 0x3f08, 0x11d3, 0x9f, 0x0b, 0x00, 0x60, 0x08, 0x03, 0x9e, 0x37);
DEFINE_GUID(CLSID_SampleGrabber, 0xc1f400a0, 0x3f08, 0x11d3, 0x9f, 0x0b, 0x00, 0x60, 0x08, 0x03, 0x9e, 0x37);


int8_t DShow_Capture::pushFrame(uint8_t *data, int32_t datalen)
{
  frameData fdata;
  fdata.data = (uint8_t*)malloc(datalen);
  memcpy(fdata.data, data, datalen);
  fdata.datalen = datalen;

  frames.push_back(fdata);
  return TRUE;
}

IPin* DShow_Capture::getPin(IBaseFilter *pFilter, LPCOLESTR pinname) {
  IEnumPins* pEnum;
  IPin* pPin;
  if (SUCCEEDED(pFilter->EnumPins(&pEnum))) {
    while (pEnum->Next(1, &pPin, 0) == S_OK) {
      PIN_INFO info;
      pPin->QueryPinInfo(&info);
      if (wcsicmp(pinname, info.achName) == 0) {
        if (info.pFilter) info.pFilter->Release();
        return pPin;
      }
      delete pPin;
    }
  }
  else {
    return NULL;
  }
}


IBaseFilter* DShow_Capture::CreateFilterFromDevice(int devId)
{
  // Create an enumerator for the category.        
  pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
  int deviceID = 0;
  while (pEnum->Next(1, &pMoniker, NULL) == S_OK)
  {
    IPropertyBag *pPropBag;
    HRESULT hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
    if (FAILED(hr))
    {
      pMoniker->Release();
      continue;
    }

    VARIANT var;
    VariantInit(&var);

    hr = TRUE;
    // Get description or friendly name.
    if (FAILED(pPropBag->Read(L"Description", &var, 0))) {
      hr = pPropBag->Read(L"FriendlyName", &var, 0);
    }
    if (SUCCEEDED(hr)) {
      if (deviceID == devId)  {
        IBaseFilter* pFilter;
        pMoniker->BindToObject(NULL, NULL, IID_IBaseFilter, (void**)&pFilter);
        VariantClear(&var);
        pPropBag->Release();
        return pFilter;
      }
      VariantClear(&var);
      deviceID++;
    }
  }
  return NULL;
}

int8_t DShow_Capture::init() {
  CoInitializeEx(NULL, COINIT_MULTITHREADED);
  if (SUCCEEDED(CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL,
    CLSCTX_INPROC_SERVER, IID_ICaptureGraphBuilder2, (void**)&pBuild))) {
    if (SUCCEEDED(CoCreateInstance(CLSID_FilterGraph, 0, CLSCTX_INPROC_SERVER,
      IID_IGraphBuilder, (void**)&pGraph))) {
      if (SUCCEEDED(CoCreateInstance(CLSID_SystemDeviceEnum, NULL,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDevEnum))))
      {
        pBuild->SetFiltergraph(pGraph);

        pGraph->QueryInterface(IID_IMediaControl, (void **)&pControl);
        pGraph->QueryInterface(IID_IMediaEvent, (void **)&pEvent);

        return TRUE;
      }
      cleanup();
      return FALSE;
    } 
    cleanup();
    return FALSE;

  }
  cleanup();
  return FALSE;

}

int8_t DShow_Capture::cleanup()
{
  if (pGraph != NULL)   pGraph->Release();
  if (pBuild != NULL)   pBuild->Release();
  if (pDevEnum != NULL) pDevEnum->Release();
  if (pEnum != NULL)    pEnum->Release();
  if (pMoniker != NULL) pMoniker->Release();
  if (pControl != NULL) pControl->Release();
  if (pEvent != NULL)   pEvent->Release();

  if (inputFilter != NULL) inputFilter->Release();
  if (pNullF != NULL)      pNullF->Release();
  if (pGrabberF != NULL)   pGrabberF->Release();
  return TRUE;
}


int8_t DShow_Capture::queryDevices() {

  devices.clear();

  // Create an enumerator for the category.        
  pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
  int deviceID = 0;
  if(pEnum == NULL)
    return 0;

  while (pEnum->Next(1, &pMoniker, NULL) == S_OK)
  {
    IPropertyBag *pPropBag;
    HRESULT hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
    if (FAILED(hr))
    {
      pMoniker->Release();
      continue;
    }

    VARIANT var;
    VariantInit(&var);

    // Get description or friendly name.
    hr = pPropBag->Read(L"Description", &var, 0);
    if (FAILED(hr))
    {
      hr = pPropBag->Read(L"FriendlyName", &var, 0);
    }
    if (SUCCEEDED(hr))
    {
      int len = ::WideCharToMultiByte(CP_ACP, 0, var.bstrVal, ::SysStringLen(var.bstrVal), NULL, 0, NULL, NULL);
      std::string dblstr(len, '\0');
      len = ::WideCharToMultiByte(CP_ACP, 0,var.bstrVal, ::SysStringLen(var.bstrVal),&dblstr[0], len,NULL, NULL);
      devices.push_back(dblstr);
      VariantClear(&var);
    }

    hr = pPropBag->Write(L"FriendlyName", &var);

    pPropBag->Release();
  }

  return TRUE;
}

int8_t DShow_Capture::queryCapabilities()
{
  device_capabilities.clear();
  IBaseFilter* filter = CreateFilterFromDevice(selectedDevice);

  if (filter == NULL) return FALSE;

  IPin* capturePin = getPin(filter,L"Capture");

  if (capturePin == NULL) return FALSE;

  IAMStreamConfig *streamConfig;
  capturePin->QueryInterface(IID_IAMStreamConfig, (void**)&streamConfig);

  int piCount;
  int piSize;

  streamConfig->GetNumberOfCapabilities(&piCount, &piSize);

  AM_MEDIA_TYPE* media;
  VIDEO_STREAM_CONFIG_CAPS video;
  VIDEOINFOHEADER *pVih = NULL;
  for (int i = 0; i < piCount; i++) {
    streamConfig->GetStreamCaps(i, &media, (BYTE*)&video);
    if (media->formattype == FORMAT_VideoInfo) {
      
      // Check the buffer size.
      if (media->cbFormat >= sizeof(VIDEOINFOHEADER)) {
        pVih = reinterpret_cast<VIDEOINFOHEADER*>(media->pbFormat);
        /* Access VIDEOINFOHEADER members through pVih. */
        double fps = 10000000.0 / pVih->AvgTimePerFrame;
        DWORD comp = pVih->bmiHeader.biCompression;
        char name[5];


        deviceCapability cap;
        cap.width = pVih->bmiHeader.biWidth;
        cap.height = pVih->bmiHeader.biHeight;
        cap.fps = fps;
        cap.format[4] = '\0';
        memcpy(cap.format, (void*)&comp, 4);
        device_capabilities.push_back(cap);
      }
    }

  }

  return TRUE;
}

int8_t DShow_Capture::setCapability(IBaseFilter* filter,int capability) {

  selectedCapability = capability;
  //IBaseFilter* filter = CreateFilterFromDevice(selectedDevice);
  if (!filter) return FALSE;

  IPin* capturePin = getPin(filter, L"Capture");

  IAMStreamConfig *streamConfig;
  capturePin->QueryInterface(IID_IAMStreamConfig, (void**)&streamConfig);

  int piCount;
  int piSize;

  streamConfig->GetNumberOfCapabilities(&piCount, &piSize);

  AM_MEDIA_TYPE* media;
  VIDEO_STREAM_CONFIG_CAPS video;
  VIDEOINFOHEADER *pVih = NULL;
  int videocapability = 0;
  for (int i = 0; i < piCount; i++) {
    streamConfig->GetStreamCaps(i, &media, (BYTE*)&video);
    if (media->formattype == FORMAT_VideoInfo) {      
      // Check the buffer size.
      if (media->cbFormat >= sizeof(VIDEOINFOHEADER)) {        
        // Select output format
        if (videocapability == capability) {
          streamConfig->SetFormat(media);
          return TRUE;
        }
        videocapability++;
      }
    }
  }

  return FALSE;
}


int8_t DShow_Capture::startCapture()
{

  inputFilter = CreateFilterFromDevice(selectedDevice);

  setCapability(inputFilter, selectedCapability);

  // Setup Sample Grabber interface
  IBaseFilter *pGrabberF = NULL;
  ISampleGrabber *pGrabber = NULL;
  if (!SUCCEEDED(CoCreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pGrabberF)))) {
    return FALSE;
  }
  pGrabberF->QueryInterface(IID_PPV_ARGS(&pGrabber));

  // Output RGB32  
  AM_MEDIA_TYPE mt;
  ZeroMemory(&mt, sizeof(mt));
  mt.majortype = MEDIATYPE_Video;
  mt.subtype = MEDIASUBTYPE_RGB32;
  if (!SUCCEEDED(pGrabber->SetMediaType(&mt))) {
    return FALSE;
  }
  
  pGrabber->SetOneShot(FALSE);
  pGrabber->SetBufferSamples(TRUE);

  if (!SUCCEEDED(CoCreateInstance(CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER,
    IID_PPV_ARGS(&pNullF)))) {
    return FALSE;
  }
  
  // Connect filters to graph
  pGraph->AddFilter(inputFilter, L"Input Filter");
  pGraph->AddFilter(pGrabberF, L"Sample Grabber");
  pGraph->AddFilter(pNullF, L"Null Filter");

  //pGraph->Connect()
  pBuild->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, inputFilter, pGrabberF, pNullF);

  pGrabber->SetCallback(new CallbackObject(), 0);

  return TRUE;
}

int8_t DShow_Capture::play()
{
  pControl->Run();
  return TRUE;
}

int8_t DShow_Capture::stop()
{

  pControl->Stop();
  return TRUE;
}

int8_t DShow_Capture::getFrame(uint8_t** data, uint32_t *size)
{
  if (!frames.size()) return FALSE;

  frameData fdata = frames.front();
  frames.pop_front();

  *data = fdata.data;
  *size = fdata.datalen;

  return TRUE;
}

int8_t DShow_Capture::selectDevice(int devId)
{
  if (devId >= 0 && devId < devices.size()) {
    selectedDevice = devId;
    return TRUE;
  }
  return FALSE;
}


extern "C" int8_t dshow_initCapture()
{
  return capture.init();
}

extern "C" int8_t dshow_queryDevices(char **devices[], int8_t *count)
{
  capture.queryDevices();

  std::vector<std::string> device_list = capture.devices;
  
  *devices = new char*[device_list.size()];

  int i = 0;
  for (std::string dev : device_list) {
    (*devices)[i] = new char[dev.length()+1];
    memcpy((*devices)[i], dev.c_str(), dev.length());
    (*devices)[i][dev.length()] = '\0';
    i++;
  }
  *count = device_list.size();

  return TRUE;
}



extern "C" int8_t dshow_selectDevice(int device)
{
  return capture.selectDevice(device);
}

extern "C" int8_t dshow_getDeviceCapabilities(deviceCapability** list, int8_t *count)
{
  capture.queryCapabilities();
  *list = new deviceCapability[capture.device_capabilities.size()];

  int i = 0;
  for (deviceCapability cap : capture.device_capabilities) {
    (*list)[i] = cap;
    i++;
  }
  *count = capture.device_capabilities.size();

  return TRUE;
}

extern "C" int8_t dshow_setDeviceCapability(int capability)
{
  capture.selectedCapability = capability;
  return TRUE;
}

extern "C" int8_t dshow_startCapture()
{
  return capture.startCapture();
}

extern "C" int8_t dshow_play()
{
  return capture.play();
}


extern "C" int8_t dshow_stop()
{
  return capture.stop();
}

extern "C" int32_t dshow_getFrameCount()
{
  return capture.getFrameCount();
}

extern "C" int8_t dshow_queryFrame(uint8_t** data, uint32_t *size)
{
  return capture.getFrame(data, size);
}
