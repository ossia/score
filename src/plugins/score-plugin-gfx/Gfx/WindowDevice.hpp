#pragma once
#include <Gfx/GfxDevice.hpp>
#include <Gfx/Window/WindowSettings.hpp>

#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QRectF>

class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QGraphicsEllipseItem;
class QGraphicsLineItem;
class QGraphicsPolygonItem;
class QLabel;
class QStackedWidget;
class QSpinBox;
class QGraphicsView;

namespace score::gfx
{
class Window;
}
namespace Gfx
{
class OutputMappingCanvas;
class OutputPreviewWindows;

class WindowProtocolFactory final : public Device::ProtocolFactory
{
  SCORE_CONCRETE("5a181207-7d40-4ad8-814e-879fcdf8cc31")
  QString prettyName() const noexcept override;
  QString category() const noexcept override;
  QUrl manual() const noexcept override;

  Device::DeviceInterface* makeDevice(
      const Device::DeviceSettings& settings,
      const Explorer::DeviceDocumentPlugin& plugin,
      const score::DocumentContext& ctx) override;
  const Device::DeviceSettings& defaultSettings() const noexcept override;
  Device::AddressDialog* makeAddAddressDialog(
      const Device::DeviceInterface& dev, const score::DocumentContext& ctx,
      QWidget* parent) override;
  Device::AddressDialog* makeEditAddressDialog(
      const Device::AddressSettings&, const Device::DeviceInterface& dev,
      const score::DocumentContext& ctx, QWidget*) override;

  Device::ProtocolSettingsWidget* makeSettingsWidget() override;

  QVariant makeProtocolSpecificSettings(const VisitorVariant& visitor) const override;

  void serializeProtocolSpecificSettings(
      const QVariant& data, const VisitorVariant& visitor) const override;

  bool checkCompatibility(
      const Device::DeviceSettings& a,
      const Device::DeviceSettings& b) const noexcept override;
};

class SCORE_PLUGIN_GFX_EXPORT WindowDevice final : public GfxOutputDevice
{
  W_OBJECT(WindowDevice)
public:
  using GfxOutputDevice::GfxOutputDevice;
  ~WindowDevice();

  score::gfx::Window* window() const noexcept;
  W_SLOT(window)

  void addAddress(const Device::FullAddressSettings& settings) override;
  void setupContextMenu(QMenu&) const override;
  ossia::net::device_base* getDevice() const override { return m_dev.get(); }
  void disconnect() override;
  bool reconnect() override;

private:
  gfx_protocol_base* m_protocol{};
  mutable std::unique_ptr<ossia::net::device_base> m_dev;
};

struct WindowSettings
{
  WindowMode mode{WindowMode::Single};
  std::vector<OutputMapping> outputs;
  int inputWidth{1920};
  int inputHeight{1080};
};

}
