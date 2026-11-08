#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_ARTNET)
#include "FixtureDialog.hpp"

#include <Protocols/Artnet/ArtnetProtocolSettingsWidget.hpp>
#include <Protocols/Artnet/DMXFixtureInstantiation.hpp>
#include <Protocols/Artnet/FixtureDatabase.hpp>

#include <score/application/ApplicationContext.hpp>

#include <Library/ItemModelFilterLineEdit.hpp>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QPlainTextEdit>
#include <QPointer>
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

  std::function<void(const QModelIndex&)> onSelectionChanged;
  void selectionChanged(
      const QItemSelection& selected, const QItemSelection& deselected) override
  {
    QTreeView::selectionChanged(selected, deselected);
    if(const auto sel = selectedIndexes(); !sel.empty() && onSelectionChanged)
      onSelectionChanged(sel.front());
  }
};

AddFixtureDialog::AddFixtureDialog(
    int startUniverse, int startAddress, ArtnetProtocolSettingsWidget& parent)
    : QDialog{&parent}
    , m_proxy{new FixtureFilterProxy{this}}
    , m_availableFixtures{new FixtureTreeView{this}}
    , m_name{this}
    , m_buttons{
          QDialogButtonBox::StandardButton::Ok
              | QDialogButtonBox::StandardButton::Cancel,
          this}
{
  auto& db = FixtureDatabase::instance();
  m_proxy->setSourceModel(&db);
  m_availableFixtures->setModel(m_proxy);

  auto search = new Library::ItemModelFilterLineEdit{*m_proxy, *m_availableFixtures, this};
  search->setPlaceholderText(tr("Search"));

  auto fixturesLayout = new QVBoxLayout;
  fixturesLayout->addWidget(search);
  fixturesLayout->addWidget(m_availableFixtures);

  this->setLayout(&m_layout);
  m_layout.addLayout(fixturesLayout);
  m_layout.addLayout(&m_setupLayoutContainer);
  m_layout.setStretch(0, 3);
  m_layout.setStretch(1, 5);

  m_availableFixtures->header()->resizeSection(0, 180);
  m_availableFixtures->onSelectionChanged = [this](const QModelIndex& idx) {
    const auto* node = m_proxy->nodeAt(idx);
    if(!node || node->parent() == &FixtureDatabase::instance().rootNode())
      return;

    updateParameters(*node);
  };

  m_count.setRange(1, 512);
  m_spacing.setRange(0, 512);
  m_channelsPerUniverse = std::max(1, parent.channelsPerUniverse());
  m_address.setRange(1, m_channelsPerUniverse);
  m_address.setValue(startAddress + 1);
  m_content.setReadOnly(true);
  m_content.setLineWrapMode(QPlainTextEdit::NoWrap);
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

void AddFixtureDialog::setCurrentFixture(QString manufacture, QString fixtureName)
{
  auto& db = FixtureDatabase::instance();
  db.onPopulated([self = QPointer{this}, manufacture, fixtureName] {
    if(!self)
      return;
    auto fixtureNode = [&]() -> const FixtureNode* {
      auto& db = FixtureDatabase::instance();

      for(auto& mf : db.m_root.children())
      {
        if(mf.name == manufacture)
        {
          for(auto& fix : mf.children())
          {
            if(fix.name == fixtureName)
            {
              return &fix;
            }
          }
          return nullptr;
        }
      }
      return nullptr;
    }();

    if(fixtureNode)
    {
      auto& db = FixtureDatabase::instance();
      auto idx = self->m_proxy->mapFromSource(db.modelIndexFromNode(*fixtureNode));
      self->m_availableFixtures->setCurrentIndex(idx);
      self->m_availableFixtures->scrollTo(idx, QAbstractItemView::PositionAtCenter);
    }
  });
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
  const int numChannels = mode.allChannels.size();
  const auto [universe, address] = Artnet::fitInUniverse(
      m_universe.value(), m_address.value() - 1, numChannels, m_channelsPerUniverse);
  if(universe <= m_universe.maximum())
  {
    m_universe.setValue(universe);
    m_address.setValue(address + 1);
  }
  m_address.setRange(1, std::max(1, m_channelsPerUniverse + 1 - numChannels));

  m_content.setPlainText(mode.content());
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

  return Artnet::repeatFixture(
      std::move(f), m_address.value() - 1, m_count.value(), m_spacing.value(),
      m_channelsPerUniverse);
}
}
#endif
