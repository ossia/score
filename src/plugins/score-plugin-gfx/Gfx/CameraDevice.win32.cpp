#include "CameraDevice.hpp"

#include <dshow.h>
namespace Gfx
{
void enumerateCameraDevices(std::function<void(CameraSettings, QString)> func)
{
  REFGUID category = CLSID_VideoInputDeviceCategory;
  IEnumMoniker* pEnum = nullptr;
  {
    // Create the System Device Enumerator.
    ICreateDevEnum* pDevEnum;
    HRESULT hr = CoCreateInstance(
        CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDevEnum));

    if(SUCCEEDED(hr))
    {
      // Create an enumerator for the category.
      hr = pDevEnum->CreateClassEnumerator(category, &pEnum, 0);
      if(hr == S_FALSE)
      {
        hr = VFW_E_NOT_FOUND; // The category is empty. Treat as an error.
      }
      pDevEnum->Release();
    }
  }

  if(pEnum)
  {
    IMoniker* pMoniker = nullptr;
    while(pEnum->Next(1, &pMoniker, NULL) == S_OK)
    {
      QString prettyName;
      CameraSettings settings;
      settings.input = "dshow";
      IPropertyBag* pPropBag;
      HRESULT hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
      if(FAILED(hr))
      {
        pMoniker->Release();
        continue;
      }

      VARIANT var;
      VariantInit(&var);

      // Get description or friendly name.
      hr = pPropBag->Read(L"Description", &var, 0);
      if(FAILED(hr))
      {
        hr = pPropBag->Read(L"FriendlyName", &var, 0);
      }
      if(SUCCEEDED(hr))
      {
        prettyName = QString::fromWCharArray(var.bstrVal);
        settings.device = "video=" + QString::fromWCharArray(var.bstrVal);
        VariantClear(&var);
      }

      hr = pPropBag->Read(L"DevicePath", &var, 0);
      if(SUCCEEDED(hr))
      {
        // The device path is not intended for display.
        // TODO why doesn't this work with ffmpeg :/
        // settings.device = "video=" + QString::fromWCharArray(var.bstrVal);
        VariantClear(&var);
      }

      if(!settings.device.isEmpty() && !prettyName.isEmpty())
      {
        func(settings, prettyName);
      }
      pPropBag->Release();
      pMoniker->Release();
    }
  }
}
}
