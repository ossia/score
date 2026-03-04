#pragma once

#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGraphicsEllipseItem>
#include <QGraphicsSceneMouseEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <qcheckbox.h>

namespace Gfx
{
class DesktopLayoutCanvas;
class OutputMappingCanvas;
class OutputPreviewWindows;

class WindowSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  explicit WindowSettingsWidget(QWidget* parent = nullptr);
  ~WindowSettingsWidget();

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

  // Soft-edge blending controls
  QDoubleSpinBox* m_blendLeftW{};
  QDoubleSpinBox* m_blendLeftG{};
  QDoubleSpinBox* m_blendRightW{};
  QDoubleSpinBox* m_blendRightG{};
  QDoubleSpinBox* m_blendTopW{};
  QDoubleSpinBox* m_blendTopG{};
  QDoubleSpinBox* m_blendBottomW{};
  QDoubleSpinBox* m_blendBottomG{};

  QComboBox* m_lockModeCombo{};
  QComboBox* m_flagCombo{};
  QComboBox* m_formatCombo{};

  int m_selectedOutput{-1};
  bool m_syncing{false}; // guard against selection sync loops

  // Two-column layout: tabs on left, inspector on right
  QTabWidget* m_tabWidget{};
  DesktopLayoutCanvas* m_desktopCanvas{};
  QWidget* m_inspectorPanel{};

  // Per-output sections (hidden when no selection)
  QWidget* m_outputSectionsContainer{};

  // Output selector button row
  QHBoxLayout* m_outputButtonsLayout{};
  void rebuildOutputButtons();
  void selectOutput(int index);

  // Preview windows
  OutputPreviewWindows* m_preview{};
  QComboBox* m_previewContentCombo{};
  void syncPreview(bool syncPositions = false);
  void syncDesktopCanvasFromMappingItem(int index);
};

}
