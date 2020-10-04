#include "CameraDevice.hpp"

#include <State/MessageListSerialization.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <score/serialization/MimeVisitor.hpp>

#include <ossia-qt/name_utils.hpp>

#include <QComboBox>
#include <QFormLayout>
#include <QMenu>
#include <QMimeData>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <wobjectimpl.h>
extern "C"
{
#include <libavdevice/avdevice.h>
#include <libavutil/opt.h>
}
#if defined(_WIN32)
#include <dshow.h>
#endif

W_OBJECT_IMPL(Gfx::CameraDevice)

namespace Gfx
{

#if defined(_WIN32)
static void enumerateDevices(std::function<void(CameraSettings, QString)> func)
{
  REFGUID category = CLSID_VideoInputDeviceCategory;
  IEnumMoniker *pEnum = nullptr;
  {
    // Create the System Device Enumerator.
    ICreateDevEnum *pDevEnum;
    HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL,
                                  CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDevEnum));

    if (SUCCEEDED(hr))
    {
      // Create an enumerator for the category.
      hr = pDevEnum->CreateClassEnumerator(category, &pEnum, 0);
      if (hr == S_FALSE)
      {
        hr = VFW_E_NOT_FOUND;  // The category is empty. Treat as an error.
      }
      pDevEnum->Release();
    }
  }

  if(pEnum)
  {
    IMoniker* pMoniker = nullptr;
    while (pEnum->Next(1, &pMoniker, NULL) == S_OK)
    {
      QString prettyName;
      CameraSettings settings;
      settings.input = "dshow";
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
        prettyName = QString::fromWCharArray(var.bstrVal);
        settings.device = "video=" + QString::fromWCharArray(var.bstrVal);
        VariantClear(&var);
      }

      hr = pPropBag->Read(L"DevicePath", &var, 0);
      if (SUCCEEDED(hr))
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
#else
static void enumerateDevices(std::function<void(CameraSettings, QString)> func)
{
  AVInputFormat* fmt = nullptr;
  while ((fmt = av_input_video_device_next(fmt)))
  {
    AVDeviceInfoList *device_list = nullptr;
    avdevice_list_input_sources(fmt, nullptr, nullptr, &device_list);

    if(device_list)
    {
      for(int i = 0; i < device_list->nb_devices; i++)
      {
        auto dev = device_list->devices[i];
        QString devname = QString("%1 (%2: %3)").arg(dev->device_name).arg(fmt->long_name).arg(fmt->name);
        // TODO see AVDeviceCapabilitiesQuery and try to show some stream info ?
        func({QString(fmt->name).split(",").front(), dev->device_name}, devname);
      }
      avdevice_free_list_devices(&device_list);
      device_list = nullptr;
    }
  }
}
#endif


CameraDevice::~CameraDevice() { }

bool CameraDevice::reconnect()
{
  disconnect();

  try
  {
    auto set = this->settings().deviceSpecificSettings.value<CameraSettings>();
    camera_settings ossia_stgs{set.input.toStdString(), set.device.toStdString(), set.size.width(), set.size.height(), set.fps};
    auto plug = m_ctx.findPlugin<DocumentPlugin>();
    if (plug)
    {
      m_protocol = new camera_protocol{plug->exec};
      m_dev = std::make_unique<camera_device>(
            ossia_stgs,
            std::unique_ptr<ossia::net::protocol_base>(m_protocol),
            this->settings().name.toStdString());
    }
    // TODOengine->reload(&proto);

    // setLogging_impl(Device::get_cur_logging(isLogging()));
  }
  catch (std::exception& e)
  {
    qDebug() << "Could not connect: " << e.what();
  }
  catch (...)
  {
    // TODO save the reason of the non-connection.
  }

  return connected();
}



class CameraEnumerator : public Device::DeviceEnumerator
{
public:
  void enumerate(std::function<void(const Device::DeviceSettings&)> f) const override
  {
    enumerateDevices([&] (const CameraSettings& set, QString name) {
      Device::DeviceSettings s;
      s.name = name;
      s.protocol = CameraProtocolFactory::static_concreteKey();
      s.deviceSpecificSettings = QVariant::fromValue(set);
      f(s);
    });
  }
};

QString CameraProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("Camera input");
}

QString CameraProtocolFactory::category() const noexcept
{
  return StandardCategories::video;
}

Device::DeviceEnumerator* CameraProtocolFactory::getEnumerator(const score::DocumentContext& ctx) const
{
  return new CameraEnumerator;
}

Device::DeviceInterface* CameraProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings,
    const score::DocumentContext& ctx)
{
  return new CameraDevice(settings, ctx);
}

const Device::DeviceSettings& CameraProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Camera";
    CameraSettings specif;
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}

Device::AddressDialog* CameraProtocolFactory::makeAddAddressDialog(
    const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::AddressDialog* CameraProtocolFactory::makeEditAddressDialog(
    const Device::AddressSettings& set,
    const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::ProtocolSettingsWidget* CameraProtocolFactory::makeSettingsWidget()
{
  return new CameraSettingsWidget;
}

QVariant CameraProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<CameraSettings>(visitor);
}

void CameraProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data,
    const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<CameraSettings>(data, visitor);
}

bool CameraProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a,
    const Device::DeviceSettings& b) const noexcept
{
  return a.name != b.name;
}

CameraSettingsWidget::CameraSettingsWidget(QWidget* parent) : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};

  auto layout = new QFormLayout;
  layout->addRow(tr("Device Name"), m_deviceNameEdit);
  setLayout(layout);

  setDefaults();
}

void CameraSettingsWidget::setDefaults()
{
  m_deviceNameEdit->setText("camera");
}

Device::DeviceSettings CameraSettingsWidget::getSettings() const
{
  Device::DeviceSettings s = m_settings;
  s.name = m_deviceNameEdit->text();
  s.protocol = CameraProtocolFactory::static_concreteKey();
  return s;
}

void CameraSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_settings = settings;

  // Clean up the name a bit
  auto prettyName = settings.name;
  if(!prettyName.isEmpty())
  {
    prettyName = prettyName.split(':').front();
    prettyName = prettyName.split('(').front();
    prettyName.remove("/dev/");
    prettyName = prettyName.trimmed();
    ossia::net::sanitize_device_name(prettyName);
  }
  m_deviceNameEdit->setText(prettyName);
}

}

template <>
void DataStreamReader::read(const Gfx::CameraSettings& n)
{
  m_stream << n.input << n.device << n.size << n.fps;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::CameraSettings& n)
{
  m_stream >> n.input >> n.device >> n.size >> n.fps;
  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::CameraSettings& n)
{
  obj["Input"] = n.input;
  obj["Device"] = n.device;
  obj["Size"] = n.size;
  obj["FPS"] = n.fps;
}

template <>
void JSONWriter::write(Gfx::CameraSettings& n)
{
  n.input = obj["Input"].toString();
  n.device = obj["Device"].toString();
  n.size <<= obj["Size"];
  n.fps = obj["FPS"].toDouble();
}
