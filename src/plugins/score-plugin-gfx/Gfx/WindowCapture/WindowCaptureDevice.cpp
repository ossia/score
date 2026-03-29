#include "WindowCaptureDevice.hpp"

#include <State/MessageListSerialization.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxInputDevice.hpp>
#include <Gfx/WindowCapture/WindowCaptureNode.hpp>

#include <score/serialization/MimeVisitor.hpp>

#include <ossia-qt/name_utils.hpp>

#include <QComboBox>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPushButton>

#include <wobjectimpl.h>

SCORE_SERALIZE_DATASTREAM_DEFINE(Gfx::WindowCapture::WindowCaptureSettings);

namespace Gfx::WindowCapture
{

// ============================================================================
// DEVICE
// ============================================================================

class WindowCaptureDevice final : public Gfx::GfxInputDevice
{
  W_OBJECT(WindowCaptureDevice)
public:
  using GfxInputDevice::GfxInputDevice;
  ~WindowCaptureDevice() { }

private:
  void disconnect() override
  {
    Gfx::GfxInputDevice::disconnect();
    auto prev = std::move(m_dev);
    m_dev = {};
    deviceChanged(prev.get(), nullptr);
  }

  bool reconnect() override
  {
    disconnect();

    try
    {
      auto set
          = this->settings().deviceSpecificSettings.value<WindowCaptureSettings>();
      auto plug = m_ctx.findPlugin<Gfx::DocumentPlugin>();
      if(plug)
      {
        auto protocol = std::make_unique<simple_texture_input_protocol>();
        m_dev = std::make_unique<simple_texture_input_device>(
            new WindowCaptureNode{set}, &plug->exec, std::move(protocol),
            this->settings().name.toStdString());
        deviceChanged(nullptr, m_dev.get());
      }
    }
    catch(std::exception& e)
    {
      qDebug() << "WindowCapture: Could not connect:" << e.what();
    }
    catch(...)
    {
    }

    return connected();
  }

  ossia::net::device_base* getDevice() const override { return m_dev.get(); }

  mutable std::unique_ptr<simple_texture_input_device> m_dev;
};

W_OBJECT_IMPL(Gfx::WindowCapture::WindowCaptureDevice)

// ============================================================================
// SETTINGS WIDGET
// ============================================================================

class WindowCaptureSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  WindowCaptureSettingsWidget(QWidget* parent = nullptr)
      : ProtocolSettingsWidget(parent)
  {
    m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
    checkForChanges(m_deviceNameEdit);
    m_deviceNameEdit->setText("Window Capture");

    m_windowList = new QComboBox{this};
    m_windowList->setEditable(false);
    m_windowList->setMinimumWidth(300);

    m_refreshBtn = new QPushButton{tr("Refresh"), this};

    m_fps = new QDoubleSpinBox{this};
    m_fps->setRange(1.0, 240.0);
    m_fps->setValue(60.0);
    m_fps->setSuffix(" fps");

    m_preview = new QLabel{this};
    m_preview->setFixedSize(320, 180);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setStyleSheet("QLabel { background-color: #222; }");

    auto layout = new QFormLayout;
    layout->addRow(tr("Device Name"), m_deviceNameEdit);
    layout->addRow(tr("Window"), m_windowList);
    layout->addRow("", m_refreshBtn);
    layout->addRow(tr("Frame Rate"), m_fps);
    layout->addRow(tr("Preview"), m_preview);

#if defined(__linux__)
    auto env = qgetenv("XDG_SESSION_TYPE");
    if(env == "wayland")
    {
      auto label = new QLabel{
          tr("On Wayland, a system picker dialog will appear when capture starts."),
          this};
      label->setWordWrap(true);
      layout->addRow(label);
    }
#endif

    setLayout(layout);

    connect(m_refreshBtn, &QPushButton::clicked, this, [this] { refreshWindows(); });
    connect(
        m_windowList, &QComboBox::currentIndexChanged, this,
        [this](int) { updatePreview(); });

    refreshWindows();
  }

  Device::DeviceSettings getSettings() const override
  {
    Device::DeviceSettings s = m_settings;
    s.name = m_deviceNameEdit->text();
    s.protocol = WindowCaptureProtocolFactory::static_concreteKey();

    WindowCaptureSettings set;
    set.fps = m_fps->value();

    int idx = m_windowList->currentIndex();
    if(idx >= 0)
    {
      set.windowId = m_windowList->currentData().toULongLong();
      set.windowTitle = m_windowList->currentText();
    }

    s.deviceSpecificSettings = QVariant::fromValue(set);
    return s;
  }

  void setSettings(const Device::DeviceSettings& settings) override
  {
    m_settings = settings;
    m_deviceNameEdit->setText(settings.name);

    if(settings.deviceSpecificSettings.canConvert<WindowCaptureSettings>())
    {
      const auto& set = settings.deviceSpecificSettings.value<WindowCaptureSettings>();
      m_fps->setValue(set.fps);

      // Try to find the window in current list
      int idx = m_windowList->findData(QVariant::fromValue(quint64(set.windowId)));
      if(idx >= 0)
        m_windowList->setCurrentIndex(idx);
    }
  }

private:
  void refreshWindows()
  {
    m_windowList->clear();
    m_previewBackend = createWindowCaptureBackend();
    if(!m_previewBackend || !m_previewBackend->available())
    {
      m_windowList->addItem(tr("(No capture backend available)"));
      return;
    }

    auto windows = m_previewBackend->enumerate();
    if(windows.empty())
    {
      m_windowList->addItem(tr("(No windows found — or portal-based capture)"));
      return;
    }

    for(const auto& w : windows)
    {
      m_windowList->addItem(
          QString::fromStdString(w.title), QVariant::fromValue(quint64(w.id)));
    }
  }

  void updatePreview()
  {
    m_preview->clear();

    int idx = m_windowList->currentIndex();
    if(idx < 0)
      return;

    uint64_t windowId = m_windowList->currentData().toULongLong();
    if(windowId == 0)
      return;

    if(!m_previewBackend || !m_previewBackend->available())
      return;

    // Grab a single frame for the preview
    if(!m_previewBackend->start(windowId))
      return;

    auto frame = m_previewBackend->grab();

    if(frame.width <= 0 || frame.height <= 0 || !frame.data
       || frame.stride <= 0)
    {
      m_previewBackend->stop();
      return;
    }

    QImage::Format qfmt = (frame.type == CapturedFrame::CPU_BGRA)
                              ? QImage::Format_ARGB32
                              : QImage::Format_RGBA8888;

    // Copy raw pixels into a QByteArray before stopping —
    // stop() frees the SHM buffer that frame.data points to.
    const int dataSize = frame.stride * frame.height;
    QByteArray pixelData(reinterpret_cast<const char*>(frame.data), dataSize);

    const int w = frame.width;
    const int h = frame.height;
    const int stride = frame.stride;

    m_previewBackend->stop();

    QImage img(
        reinterpret_cast<const uchar*>(pixelData.constData()),
        w, h, stride, qfmt);

    QPixmap pix = QPixmap::fromImage(img).scaled(
        m_preview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_preview->setPixmap(pix);
  }

  Device::DeviceSettings m_settings;
  QLineEdit* m_deviceNameEdit{};
  QComboBox* m_windowList{};
  QPushButton* m_refreshBtn{};
  QDoubleSpinBox* m_fps{};
  QLabel* m_preview{};
  std::unique_ptr<WindowCaptureBackend> m_previewBackend;
};

// ============================================================================
// FACTORY
// ============================================================================

QString WindowCaptureProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("Window Capture");
}

QString WindowCaptureProtocolFactory::category() const noexcept
{
  return StandardCategories::video_in;
}

QUrl WindowCaptureProtocolFactory::manual() const noexcept
{
  return {};
}

Device::DeviceEnumerators
WindowCaptureProtocolFactory::getEnumerators(const score::DocumentContext& ctx) const
{
  return {};
}

Device::DeviceInterface* WindowCaptureProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings,
    const Explorer::DeviceDocumentPlugin& plugin, const score::DocumentContext& ctx)
{
  return new WindowCaptureDevice(settings, ctx);
}

const Device::DeviceSettings&
WindowCaptureProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Window Capture";
    WindowCaptureSettings specif;
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}

Device::AddressDialog* WindowCaptureProtocolFactory::makeAddAddressDialog(
    const Device::DeviceInterface& dev, const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::AddressDialog* WindowCaptureProtocolFactory::makeEditAddressDialog(
    const Device::AddressSettings& set, const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx, QWidget* parent)
{
  return nullptr;
}

Device::ProtocolSettingsWidget* WindowCaptureProtocolFactory::makeSettingsWidget()
{
  return new WindowCaptureSettingsWidget;
}

QVariant WindowCaptureProtocolFactory::makeProtocolSpecificSettings(
    const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<WindowCaptureSettings>(visitor);
}

void WindowCaptureProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<WindowCaptureSettings>(data, visitor);
}

bool WindowCaptureProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a,
    const Device::DeviceSettings& b) const noexcept
{
  return true;
}

}

// ============================================================================
// SERIALIZATION
// ============================================================================

template <>
void DataStreamReader::read(const Gfx::WindowCapture::WindowCaptureSettings& n)
{
  m_stream << n.windowTitle << n.windowId << n.fps;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::WindowCapture::WindowCaptureSettings& n)
{
  m_stream >> n.windowTitle >> n.windowId >> n.fps;
  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::WindowCapture::WindowCaptureSettings& n)
{
  obj["WindowTitle"] = n.windowTitle;
  obj["WindowId"] = (int64_t)n.windowId;
  obj["FPS"] = n.fps;
}

template <>
void JSONWriter::write(Gfx::WindowCapture::WindowCaptureSettings& n)
{
  n.windowTitle = obj["WindowTitle"].toString();
  n.windowId = obj["WindowId"].toUInt64();
  n.fps = obj["FPS"].toDouble();
}
