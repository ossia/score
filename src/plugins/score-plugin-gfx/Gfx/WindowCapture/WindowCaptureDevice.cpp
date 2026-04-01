#include "WindowCaptureDevice.hpp"

#include <State/MessageListSerialization.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxInputDevice.hpp>
#include <Gfx/WindowCapture/WindowCaptureNode.hpp>

#include <score/serialization/MimeVisitor.hpp>

#include <ossia-qt/name_utils.hpp>

#include <QComboBox>
#include <QTimer>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPalette>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardItemModel>

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

    // ── Mode selector ──
    m_modeCombo = new QComboBox{this};
    m_modeCombo->addItem(tr("Window"), (int)CaptureMode::Window);
    m_modeCombo->addItem(tr("All Screens"), (int)CaptureMode::AllScreens);
    m_modeCombo->addItem(tr("Single Screen"), (int)CaptureMode::SingleScreen);
    m_modeCombo->addItem(tr("Region"), (int)CaptureMode::Region);

    // ── Window selection (for Window mode) ──
    m_windowList = new QComboBox{this};
    m_windowList->setEditable(false);
    m_windowList->setMinimumWidth(300);
    m_windowLabel = new QLabel{tr("Window"), this};

    // ── Screen selection (for SingleScreen mode) ──
    m_screenList = new QComboBox{this};
    m_screenList->setEditable(false);
    m_screenList->setMinimumWidth(300);
    m_screenLabel = new QLabel{tr("Screen"), this};

    // ── Region (for Region mode) ──
    m_regionX = new QSpinBox{this};
    m_regionX->setRange(0, 32768);
    m_regionY = new QSpinBox{this};
    m_regionY->setRange(0, 32768);
    m_regionW = new QSpinBox{this};
    m_regionW->setRange(1, 32768);
    m_regionW->setValue(1920);
    m_regionH = new QSpinBox{this};
    m_regionH->setRange(1, 32768);
    m_regionH->setValue(1080);
    m_regionXLabel = new QLabel{tr("Region X"), this};
    m_regionYLabel = new QLabel{tr("Region Y"), this};
    m_regionWLabel = new QLabel{tr("Region Width"), this};
    m_regionHLabel = new QLabel{tr("Region Height"), this};

    m_refreshBtn = new QPushButton{tr("Refresh"), this};

    m_fps = new QDoubleSpinBox{this};
    m_fps->setRange(1.0, 240.0);
    m_fps->setValue(60.0);
    m_fps->setSuffix(" fps");

    m_preview = new QLabel{this};
    m_preview->setFixedSize(320, 180);
    m_preview->setAlignment(Qt::AlignCenter);
    {
      QPalette pal = m_preview->palette();
      pal.setColor(QPalette::Window, QColor(0x22, 0x22, 0x22));
      m_preview->setAutoFillBackground(true);
      m_preview->setPalette(pal);
    }

    auto layout = new QFormLayout;
    layout->addRow(tr("Device Name"), m_deviceNameEdit);
    layout->addRow(tr("Mode"), m_modeCombo);
    layout->addRow(m_windowLabel, m_windowList);
    layout->addRow(m_screenLabel, m_screenList);
    layout->addRow(m_regionXLabel, m_regionX);
    layout->addRow(m_regionYLabel, m_regionY);
    layout->addRow(m_regionWLabel, m_regionW);
    layout->addRow(m_regionHLabel, m_regionH);
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

    m_debounceTimer = new QTimer{this};
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(100);
    connect(m_debounceTimer, &QTimer::timeout, this, [this] { updatePreview(); });

    connect(
        m_modeCombo, &QComboBox::currentIndexChanged, this,
        [this](int) { onModeChanged(); });
    connect(m_refreshBtn, &QPushButton::clicked, this, [this] { refreshLists(); });
    connect(
        m_windowList, &QComboBox::currentIndexChanged, this,
        [this](int) { m_debounceTimer->start(); });
    connect(
        m_screenList, &QComboBox::currentIndexChanged, this,
        [this](int) { m_debounceTimer->start(); });
    for(auto* spin : {m_regionX, m_regionY, m_regionW, m_regionH})
      connect(spin, &QSpinBox::valueChanged, this, [this] { m_debounceTimer->start(); });

    // Permission warning (hidden by default)
    m_permissionWarning = new QLabel{this};
    m_permissionWarning->setWordWrap(true);
    m_permissionWarning->setVisible(false);
    {
      QPalette pal = m_permissionWarning->palette();
      pal.setColor(QPalette::WindowText, QColor(0xCC, 0x44, 0x44));
      m_permissionWarning->setPalette(pal);
    }
    layout->addRow(m_permissionWarning);

    // Initialize backend and populate lists
    m_previewBackend = createWindowCaptureBackend();
    if(!m_previewBackend || !m_previewBackend->available())
    {
      m_permissionWarning->setText(
          tr("Screen capture is unavailable. On macOS, grant Screen Recording "
             "permission in System Settings > Privacy & Security > Screen "
             "Recording. On other platforms, ensure the required capture "
             "libraries are installed."));
      m_permissionWarning->setVisible(true);
    }
    updateModeAvailability();
    onModeChanged();
    refreshLists();
  }

  Device::DeviceSettings getSettings() const override
  {
    Device::DeviceSettings s = m_settings;
    s.name = m_deviceNameEdit->text();
    s.protocol = WindowCaptureProtocolFactory::static_concreteKey();

    WindowCaptureSettings set;
    set.mode = static_cast<CaptureMode>(
        m_modeCombo->currentData().toInt());
    set.fps = m_fps->value();

    switch(set.mode)
    {
      case CaptureMode::Window:
      {
        int idx = m_windowList->currentIndex();
        if(idx >= 0)
        {
          set.windowId = m_windowList->currentData().toULongLong();
          set.windowTitle = m_windowList->currentText();
        }
        break;
      }
      case CaptureMode::AllScreens:
        break;
      case CaptureMode::SingleScreen:
      {
        int idx = m_screenList->currentIndex();
        if(idx >= 0)
        {
          set.screenId = m_screenList->currentData().toULongLong();
          set.screenName = m_screenList->currentText();
        }
        break;
      }
      case CaptureMode::Region:
        set.regionX = m_regionX->value();
        set.regionY = m_regionY->value();
        set.regionW = m_regionW->value();
        set.regionH = m_regionH->value();
        break;
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

      // Set mode
      int modeIdx = m_modeCombo->findData((int)set.mode);
      if(modeIdx >= 0)
        m_modeCombo->setCurrentIndex(modeIdx);

      // Try to restore window selection
      int wIdx = m_windowList->findData(QVariant::fromValue(quint64(set.windowId)));
      if(wIdx >= 0)
        m_windowList->setCurrentIndex(wIdx);

      // Try to restore screen selection
      int sIdx = m_screenList->findData(QVariant::fromValue(quint64(set.screenId)));
      if(sIdx >= 0)
        m_screenList->setCurrentIndex(sIdx);

      // Restore region
      m_regionX->setValue(set.regionX);
      m_regionY->setValue(set.regionY);
      m_regionW->setValue(set.regionW);
      m_regionH->setValue(set.regionH);
    }
  }

private:
  void updateModeAvailability()
  {
    // Disable modes not supported by the current backend
    if(!m_previewBackend || !m_previewBackend->available())
      return;

    for(int i = 0; i < m_modeCombo->count(); i++)
    {
      auto mode = static_cast<CaptureMode>(m_modeCombo->itemData(i).toInt());
      bool supported = m_previewBackend->supportsMode(mode);
      auto* model = qobject_cast<QStandardItemModel*>(m_modeCombo->model());
      if(model)
      {
        auto* item = model->item(i);
        if(item)
        {
          item->setEnabled(supported);
          if(!supported)
            item->setToolTip(tr("Not supported by the current capture backend"));
        }
      }
    }
  }

  void onModeChanged()
  {
    auto mode = static_cast<CaptureMode>(
        m_modeCombo->currentData().toInt());

    bool showWindow = (mode == CaptureMode::Window);
    bool showScreen = (mode == CaptureMode::SingleScreen);
    bool showRegion = (mode == CaptureMode::Region);
    bool showRefresh = showWindow || showScreen;

    m_windowLabel->setVisible(showWindow);
    m_windowList->setVisible(showWindow);
    m_screenLabel->setVisible(showScreen);
    m_screenList->setVisible(showScreen);
    m_regionXLabel->setVisible(showRegion);
    m_regionX->setVisible(showRegion);
    m_regionYLabel->setVisible(showRegion);
    m_regionY->setVisible(showRegion);
    m_regionWLabel->setVisible(showRegion);
    m_regionW->setVisible(showRegion);
    m_regionHLabel->setVisible(showRegion);
    m_regionH->setVisible(showRegion);
    m_refreshBtn->setVisible(showRefresh);
    m_preview->setVisible(showWindow || showScreen || showRegion);

    m_debounceTimer->start();
  }

  void refreshLists()
  {
    if(!m_previewBackend)
      m_previewBackend = createWindowCaptureBackend();
    if(!m_previewBackend || !m_previewBackend->available())
    {
      m_permissionWarning->setText(
          tr("Screen capture is unavailable. On macOS, grant Screen Recording "
             "permission in System Settings > Privacy & Security > Screen "
             "Recording. On other platforms, ensure the required capture "
             "libraries are installed."));
      m_permissionWarning->setVisible(true);
      return;
    }

    refreshWindows();
    refreshScreens();

    // On macOS, ScreenCaptureKit may report as available but return
    // empty lists if the app lacks Screen Recording permission.
    if(m_windowList->count() == 0
       || (m_windowList->count() == 1
           && m_windowList->currentData().toULongLong() == 0))
    {
      m_permissionWarning->setText(
          tr("No windows or screens found. On macOS, ensure Screen Recording "
             "permission is granted in System Settings > Privacy & Security > "
             "Screen Recording, then click Refresh."));
      m_permissionWarning->setVisible(true);
    }
    else
    {
      m_permissionWarning->setVisible(false);
    }
  }

  void refreshWindows()
  {
    m_windowList->clear();

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

  void refreshScreens()
  {
    m_screenList->clear();

    if(!m_previewBackend || !m_previewBackend->available())
    {
      m_screenList->addItem(tr("(No capture backend available)"));
      return;
    }

    auto screens = m_previewBackend->enumerateScreens();
    if(screens.empty())
    {
      m_screenList->addItem(tr("(No screens found — or portal-based capture)"));
      return;
    }

    for(const auto& s : screens)
    {
      QString label = QString::fromStdString(s.name);
      if(s.width > 0 && s.height > 0)
        label += QStringLiteral(" (%1x%2)").arg(s.width).arg(s.height);
      m_screenList->addItem(label, QVariant::fromValue(quint64(s.id)));
    }
  }

  void updatePreview()
  {
    m_preview->clear();
    stopPreviewTimer();

    if(!m_previewBackend || !m_previewBackend->available())
      return;

    auto mode = static_cast<CaptureMode>(
        m_modeCombo->currentData().toInt());

    CaptureTarget target;
    target.mode = mode;

    switch(mode)
    {
      case CaptureMode::Window:
      {
        int idx = m_windowList->currentIndex();
        if(idx < 0)
          return;
        target.windowId = m_windowList->currentData().toULongLong();
        if(target.windowId == 0)
          return;
        break;
      }
      case CaptureMode::SingleScreen:
      {
        int idx = m_screenList->currentIndex();
        if(idx < 0)
          return;
        target.screenId = m_screenList->currentData().toULongLong();
        if(target.screenId == 0)
          return;
        break;
      }
      case CaptureMode::Region:
      {
        target.regionX = m_regionX->value();
        target.regionY = m_regionY->value();
        target.regionW = m_regionW->value();
        target.regionH = m_regionH->value();
        if(target.regionW <= 0 || target.regionH <= 0)
          return;
        break;
      }
      default:
        return;
    }

    // Grab a single frame for the preview
    if(!m_previewBackend->start(target))
      return;

    // Capture backends deliver frames asynchronously — the first frame
    // is not available immediately after start().  Retry via a timer
    // on the event loop so we don't block the UI thread.
    m_previewRetries = 0;
    m_previewTimer = new QTimer{this};
    m_previewTimer->setInterval(16);
    connect(m_previewTimer, &QTimer::timeout, this, [this] {
      tryGrabPreview();
    });
    m_previewTimer->start();
  }

  void tryGrabPreview()
  {
    ++m_previewRetries;

    auto frame = m_previewBackend->grab();

    if(frame.type == CapturedFrame::None || !frame.data
       || frame.width <= 0 || frame.height <= 0 || frame.stride <= 0)
    {
      if(m_previewRetries >= 60) // ~1 s total
      {
        m_previewBackend->stop();
        stopPreviewTimer();
      }
      return;
    }

    // Got a valid frame — display it and clean up
    QImage::Format qfmt = (frame.type == CapturedFrame::CPU_BGRA)
                              ? QImage::Format_ARGB32
                              : QImage::Format_RGBA8888;

    // Copy raw pixels before stopping —
    // stop() frees the buffer that frame.data points to.
    const int dataSize = frame.stride * frame.height;
    QByteArray pixelData(reinterpret_cast<const char*>(frame.data), dataSize);

    const int w = frame.width;
    const int h = frame.height;
    const int stride = frame.stride;

    m_previewBackend->stop();
    stopPreviewTimer();

    QImage img(
        reinterpret_cast<const uchar*>(pixelData.constData()),
        w, h, stride, qfmt);

    QPixmap pix = QPixmap::fromImage(img).scaled(
        m_preview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_preview->setPixmap(pix);
  }

  void stopPreviewTimer()
  {
    if(m_previewTimer)
    {
      m_previewTimer->stop();
      delete m_previewTimer;
      m_previewTimer = nullptr;
    }
    m_previewRetries = 0;
  }

  Device::DeviceSettings m_settings;
  QLineEdit* m_deviceNameEdit{};

  QComboBox* m_modeCombo{};

  QLabel* m_windowLabel{};
  QComboBox* m_windowList{};

  QLabel* m_screenLabel{};
  QComboBox* m_screenList{};

  QLabel* m_regionXLabel{};
  QSpinBox* m_regionX{};
  QLabel* m_regionYLabel{};
  QSpinBox* m_regionY{};
  QLabel* m_regionWLabel{};
  QSpinBox* m_regionW{};
  QLabel* m_regionHLabel{};
  QSpinBox* m_regionH{};

  QPushButton* m_refreshBtn{};
  QDoubleSpinBox* m_fps{};
  QLabel* m_preview{};
  QLabel* m_permissionWarning{};
  QTimer* m_debounceTimer{};
  QTimer* m_previewTimer{};
  int m_previewRetries{};
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
  m_stream << (int)n.mode << n.windowTitle << n.windowId << n.screenId
           << n.screenName << n.regionX << n.regionY << n.regionW << n.regionH
           << n.fps;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::WindowCapture::WindowCaptureSettings& n)
{
  int mode{};
  m_stream >> mode >> n.windowTitle >> n.windowId >> n.screenId >> n.screenName
      >> n.regionX >> n.regionY >> n.regionW >> n.regionH >> n.fps;
  n.mode = static_cast<Gfx::WindowCapture::CaptureMode>(mode);
  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::WindowCapture::WindowCaptureSettings& n)
{
  obj["Mode"] = (int)n.mode;
  obj["WindowTitle"] = n.windowTitle;
  obj["WindowId"] = (int64_t)n.windowId;
  obj["ScreenId"] = (int64_t)n.screenId;
  obj["ScreenName"] = n.screenName;
  obj["RegionX"] = n.regionX;
  obj["RegionY"] = n.regionY;
  obj["RegionW"] = n.regionW;
  obj["RegionH"] = n.regionH;
  obj["FPS"] = n.fps;
}

template <>
void JSONWriter::write(Gfx::WindowCapture::WindowCaptureSettings& n)
{
  n.mode = static_cast<Gfx::WindowCapture::CaptureMode>(
      obj["Mode"].toInt());
  n.windowTitle = obj["WindowTitle"].toString();
  n.windowId = obj["WindowId"].toUInt64();
  n.screenId = obj["ScreenId"].toUInt64();
  n.screenName = obj["ScreenName"].toString();
  n.regionX = obj["RegionX"].toInt();
  n.regionY = obj["RegionY"].toInt();
  n.regionW = obj["RegionW"].toInt();
  n.regionH = obj["RegionH"].toInt();
  n.fps = obj["FPS"].toDouble();
}
