#pragma once

#include <Device/Protocol/ProtocolSettingsWidget.hpp>

namespace Gfx
{
class LibavSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  explicit LibavSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

private:
  void onDirectionChanged();
  void onMuxerChanged();
  void onVEncoderChanged();
  void onAEncoderChanged();

  QLineEdit* m_deviceNameEdit{};
  QComboBox* m_direction{};
  QStackedWidget* m_stack{};

  // Input page widgets
  QLineEdit* m_inPath{};
  QPlainTextEdit* m_inOptions{};

  // Output page widgets
  QLineEdit* m_outPath{};
  QSpinBox* m_width{};
  QSpinBox* m_height{};
  QSpinBox* m_rate{};
  QSpinBox* m_audioChannels{};
  QComboBox* m_muxer{};
  QComboBox* m_vencoder{};
  QComboBox* m_aencoder{};
  QComboBox* m_pixfmt{};
  QComboBox* m_smpfmt{};
  QPlainTextEdit* m_outOptions{};
  QPushButton* m_showOptions{};
  QComboBox* m_inputTransfer{};
  QLabel* m_validationLabel{};

  Device::DeviceSettings m_settings;

  void validateOutput();
};
}