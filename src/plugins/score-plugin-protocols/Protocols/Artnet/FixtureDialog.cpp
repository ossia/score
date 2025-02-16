#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_ARTNET)
#include "FixtureDialog.hpp"

#include <Protocols/Artnet/ArtnetProtocolSettingsWidget.hpp>
#include <Protocols/Artnet/FixtureDatabase.hpp>

#include <score/application/ApplicationContext.hpp>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QVariant>

namespace Protocols
{

class FixtureTreeView : public QTreeView
{
public:
  FixtureTreeView(QWidget* parent = nullptr)
      : QTreeView{parent}
  {
    setAllColumnsShowFocus(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setDragDropMode(QAbstractItemView::NoDragDrop);
    setAlternatingRowColors(true);
    setUniformRowHeights(true);
  }

  std::function<void(const FixtureNode&)> onSelectionChanged;
  void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
  {
    auto sel = this->selectedIndexes();
    if(!sel.empty())
    {
      auto obj = (FixtureNode*)sel.at(0).internalPointer();
      if(obj)
      {
        onSelectionChanged(*obj);
      }
    }
  }
};

AddFixtureDialog::AddFixtureDialog(
    int startUniverse, int startAddress, ArtnetProtocolSettingsWidget& parent)
    : QDialog{&parent}
    , m_availableFixtures{new FixtureTreeView{this}}
    , m_name{this}
    , m_buttons{
          QDialogButtonBox::StandardButton::Ok
              | QDialogButtonBox::StandardButton::Cancel,
          this}
{
  this->setLayout(&m_layout);
  m_layout.addWidget(m_availableFixtures);
  m_layout.addLayout(&m_setupLayoutContainer);
  m_layout.setStretch(0, 3);
  m_layout.setStretch(1, 5);

  auto* db = &FixtureDatabase::instance();
  m_availableFixtures->setModel(db);
  m_availableFixtures->header()->resizeSection(0, 180);
  m_availableFixtures->onSelectionChanged = [&](const FixtureNode& newFixt) {
    // Manufacturer, do nothing
    if(newFixt.childCount() > 0)
      return;

    updateParameters(newFixt);
  };

  m_count.setRange(1, 512);
  m_spacing.setRange(0, 512);
  m_address.setRange(1, 512);
  m_address.setValue(startAddress);
  auto [umin, umax] = parent.universeRange();
  m_universe.setRange(umin, umax);
  m_universe.setValue(startUniverse);

  m_setupLayoutContainer.addLayout(&m_setupLayout);
  m_setupLayout.addRow(tr("Name"), &m_name);
  m_setupLayout.addRow(tr("Address"), &m_address);
  m_setupLayout.addRow(tr("Count"), &m_count);
  m_setupLayout.addRow(tr("Spacing"), &m_spacing);
  m_setupLayout.addRow(tr("Universe"), &m_universe);
  m_setupLayout.addRow(tr("Mode"), &m_mode);
  m_setupLayout.addRow(tr("Channels"), &m_content);
  m_setupLayoutContainer.addStretch(0);
  m_setupLayoutContainer.addWidget(&m_buttons);

  connect(&m_buttons, &QDialogButtonBox::accepted, this, &AddFixtureDialog::accept);
  connect(&m_buttons, &QDialogButtonBox::rejected, this, &AddFixtureDialog::reject);

  connect(
      &m_mode, qOverload<int>(&QComboBox::currentIndexChanged), this,
      &AddFixtureDialog::setMode);
}

void AddFixtureDialog::updateParameters(const FixtureNode& fixt)
{
  m_name.setText(fixt.name);

  m_mode.clear();
  for(auto& mode : fixt.modes)
    m_mode.addItem(mode.name);
  m_mode.setCurrentIndex(0);

  m_currentFixture = &fixt;

  setMode(0);
}

void AddFixtureDialog::setMode(int mode_index)
{
  if(!m_currentFixture)
    return;
  if(!ossia::valid_index(mode_index, m_currentFixture->modes))
    return;

  const FixtureMode& mode = m_currentFixture->modes[mode_index];
  int numChannels = mode.allChannels.size();
  m_address.setRange(1, 513 - numChannels);

  m_content.setText(mode.content());
}

QSize AddFixtureDialog::sizeHint() const
{
  return QSize{800, 600};
}

std::vector<Artnet::Fixture> AddFixtureDialog::fixtures() const noexcept
{
  std::vector<Artnet::Fixture> res;

  if(!m_currentFixture)
    return res;

  int mode_index = m_mode.currentIndex();
  if(!ossia::valid_index(mode_index, m_currentFixture->modes))
    return res;

  auto& mode = m_currentFixture->modes[mode_index];

  Artnet::Fixture f;
  f.fixtureName = m_name.text();
  f.controls = mode.channels;
  if(f.fixtureName.isEmpty() || f.controls.empty())
    return res;

  f.modeName = m_mode.currentText();
  f.mode.channelNames = mode.allChannels;
  f.universe = m_universe.value();

  for(int i = 0; i < m_count.value(); i++)
  {
    f.address
        = m_address.value() - 1 + i * (f.mode.channelNames.size() + m_spacing.value());
    res.push_back(f);
  }

  return res;
}
}
#endif
