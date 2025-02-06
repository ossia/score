#pragma once
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_ARTNET)
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Protocols/Artnet/ArtnetSpecificSettings.hpp>

#include <score/model/tree/TreeNode.hpp>

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>

namespace Protocols
{
class FixtureTreeView;
class FixtureData;

using FixtureNode = TreeNode<FixtureData>;
class ArtnetProtocolSettingsWidget;
class AddFixtureDialog : public QDialog
{
public:
  explicit AddFixtureDialog(ArtnetProtocolSettingsWidget& parent);

  void updateParameters(const FixtureNode& fixt);
  void setMode(int mode_index);
  QString name() const noexcept { return m_name.text(); }
  void setName(QString t) { m_name.setText(t); }

  QSize sizeHint() const override;
  Artnet::Fixture fixture() const noexcept;

private:
  QHBoxLayout m_layout;
  FixtureTreeView* m_availableFixtures{};

  QVBoxLayout m_setupLayoutContainer;
  QFormLayout m_setupLayout;
  State::AddressFragmentLineEdit m_name;
  QSpinBox m_address;
  QComboBox m_mode;
  QLabel m_content;
  QDialogButtonBox m_buttons;

  const FixtureData* m_currentFixture{};
};

}
#endif
