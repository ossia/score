#pragma once
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_ARTNET)
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Protocols/Artnet/ArtnetSpecificSettings.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QSpinBox>

namespace Protocols
{
class ArtnetProtocolSettingsWidget;
class AddLEDStripDialog : public QDialog
{
public:
  explicit AddLEDStripDialog(ArtnetProtocolSettingsWidget& parent);
  Artnet::Fixture fixture() const noexcept;

private:
  void on_channelsChanged(int count);
  QFormLayout m_layout;
  State::AddressFragmentLineEdit m_name;
  QSpinBox m_address;
  QSpinBox m_channels;
  QHBoxLayout m_channelComboLayout;
  std::vector<QComboBox*> m_channelCombos;
  QSpinBox m_pixels;
  QCheckBox m_reverse;

  QDialogButtonBox m_buttons;
};
}
#endif
