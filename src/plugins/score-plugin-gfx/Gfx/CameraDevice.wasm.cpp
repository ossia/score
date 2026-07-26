#include "CameraDevice.hpp"

#include <Video/WebCameraInput.hpp>

#include <Gfx/CameraDeviceEnumerator.hpp>

#include <QTimer>

#include <algorithm>

namespace Gfx
{
namespace
{
constexpr auto webcam_input_kind = "webcam";

using WebCameraEntry = std::pair<QString, QString>;

Device::DeviceSettings toDeviceSettings(const WebCameraEntry& e)
{
  CameraSettings set;
  set.input = webcam_input_kind;
  set.device = e.first;

  Device::DeviceSettings s;
  s.name = e.second;
  s.protocol = CameraProtocolFactory::static_concreteKey();
  s.deviceSpecificSettings = QVariant::fromValue(set);
  return s;
}

class WebCameraEnumerator final : public Device::DeviceEnumerator
{
public:
  WebCameraEnumerator()
  {
    m_timer.setInterval(250);
    connect(&m_timer, &QTimer::timeout, this, &WebCameraEnumerator::poll);
  }

  void enumerate(std::function<void(const QString&, const Device::DeviceSettings&)> f)
      const override
  {
    for(const auto& e : m_known)
      f(e.second, toDeviceSettings(e));

    // enumerateDevices() is asynchronous, and the browser hands out neither ids
    // nor labels until camera permission has been granted once: the list has to
    // be rescanned rather than read once.
    ::Video::webCameraScan();
    m_timer.start();
  }

private:
  void poll()
  {
    const int gen = ::Video::webCameraScanGeneration();
    if(gen == m_generation)
      return;
    m_generation = gen;

    std::vector<WebCameraEntry> found;
    int anonymous = 0;
    for(const auto& dev : ::Video::webCameraDevices())
    {
      QString name = QString::fromStdString(dev.label);
      if(name.isEmpty())
        name = tr("Camera %1 (allow access to see its name)").arg(++anonymous);
      found.push_back({QString::fromStdString(dev.id), name});
    }

    for(const auto& e : m_known)
      if(std::find(found.begin(), found.end(), e) == found.end())
        deviceRemoved(e.second);

    for(const auto& e : found)
      if(std::find(m_known.begin(), m_known.end(), e) == m_known.end())
        deviceAdded(e.second, toDeviceSettings(e));

    m_known = std::move(found);
    sort();
  }

  mutable QTimer m_timer;
  std::vector<WebCameraEntry> m_known;
  int m_generation{-1};
};

class WebCameraDeviceEnumerator final : public CameraDeviceEnumerator
{
public:
  void registerAllEnumerators(Device::DeviceEnumerators& enums) override
  {
    enums.push_back({"Cameras", new WebCameraEnumerator});
  }
};
}

std::shared_ptr<CameraDeviceEnumerator> make_camera_enumerator()
{
  return std::make_shared<WebCameraDeviceEnumerator>();
}

void enumerateCameraDevices(std::function<void(CameraSettings, QString)> func)
{
  ::Video::webCameraScan();
  for(const auto& dev : ::Video::webCameraDevices())
  {
    CameraSettings set;
    set.input = webcam_input_kind;
    set.device = QString::fromStdString(dev.id);
    func(set, QString::fromStdString(dev.label));
  }
}
}
