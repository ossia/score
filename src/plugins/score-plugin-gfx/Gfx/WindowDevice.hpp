#pragma once
#include <Gfx/GfxDevice.hpp>

#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QRectF>

class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
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

struct WindowOutputSettings
{
  int width{};
  int height{};
  double rate{};
  bool viewportSize{};
  bool vsync{};
};

enum class WindowMode : int
{
  Single = 0,
  Background = 1,
  MultiWindow = 2
};

struct OutputMapping
{
  QRectF sourceRect{0.0, 0.0, 1.0, 1.0}; // UV coords in input texture
  int screenIndex{-1};                     // -1 = default screen
  QPoint windowPosition{0, 0};
  QSize windowSize{1280, 720};
  bool fullscreen{false};
};

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

private:
  void addAddress(const Device::FullAddressSettings& settings) override;
  void setupContextMenu(QMenu&) const override;
  ossia::net::device_base* getDevice() const override { return m_dev.get(); }
  void disconnect() override;
  bool reconnect() override;

  gfx_protocol_base* m_protocol{};
  mutable std::unique_ptr<ossia::net::device_base> m_dev;
};

struct WindowSettings
{
  WindowMode mode{WindowMode::Single};
  std::vector<OutputMapping> outputs;
};

// Graphics item for draggable/resizable output mapping quads
class OutputMappingItem final : public QGraphicsRectItem
{
public:
  explicit OutputMappingItem(int index, const QRectF& rect, QGraphicsItem* parent = nullptr);

  int outputIndex() const noexcept { return m_index; }
  void setOutputIndex(int idx);

  // Per-output window properties stored on the item
  int screenIndex{-1};
  QPoint windowPosition{0, 0};
  QSize windowSize{1280, 720};
  bool fullscreen{false};

  // Called when item is moved or resized in the canvas
  std::function<void()> onChanged;

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

protected:
  QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;

private:
  enum ResizeEdge
  {
    None = 0,
    Left = 1,
    Right = 2,
    Top = 4,
    Bottom = 8
  };
  int hitTestEdges(const QPointF& pos) const;

  int m_index{};
  int m_resizeEdges{None};
  QPointF m_dragStart{};
  QRectF m_rectStart{};
};

class OutputMappingCanvas final : public QGraphicsView
{
public:
  explicit OutputMappingCanvas(QWidget* parent = nullptr);

  void setMappings(const std::vector<OutputMapping>& mappings);
  std::vector<OutputMapping> getMappings() const;

  void addOutput();
  void removeSelectedOutput();

  static constexpr double kCanvasWidth = 400.0;
  static constexpr double kCanvasHeight = 300.0;

  // Signal-like: call this when selection changes
  std::function<void(int)> onSelectionChanged;
  // Called when a mapping item's geometry changes (move/resize)
  std::function<void(int)> onItemGeometryChanged;

protected:
  void resizeEvent(QResizeEvent* event) override;

private:
  void setupItemCallbacks(OutputMappingItem* item);
  QGraphicsScene m_scene;
};

class WindowSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  WindowSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;

  void setSettings(const Device::DeviceSettings& settings) override;

private:
  void onModeChanged(int index);
  void onSelectionChanged(int outputIndex);
  void updatePropertiesFromSelection();
  void applyPropertiesToSelection();
  void applySourceRectToSelection();
  void updatePixelLabels();

  QLineEdit* m_deviceNameEdit{};
  QComboBox* m_modeCombo{};
  QStackedWidget* m_stack{};

  // Multi-window UI
  OutputMappingCanvas* m_canvas{};
  QSpinBox* m_winPosX{};
  QSpinBox* m_winPosY{};
  QSpinBox* m_winWidth{};
  QSpinBox* m_winHeight{};
  QComboBox* m_screenCombo{};
  QCheckBox* m_fullscreenCheck{};
  QDoubleSpinBox* m_srcX{};
  QDoubleSpinBox* m_srcY{};
  QDoubleSpinBox* m_srcW{};
  QDoubleSpinBox* m_srcH{};
  QLabel* m_srcPosPixelLabel{};
  QLabel* m_srcSizePixelLabel{};
  QSpinBox* m_inputWidth{};
  QSpinBox* m_inputHeight{};
  int m_selectedOutput{-1};
};

}
