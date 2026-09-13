// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MCUProtocolSettingsWidget.hpp"

#include "MCUProtocolFactory.hpp"
#include "MCUSpecificSettings.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Protocols/MIDIDevices/MidiDeviceDatabase.hpp>
#include <Protocols/MIDIUtils.hpp>

#include <score/widgets/ComboBox.hpp>
#include <score/widgets/MarginLess.hpp>

#include <ossia-qt/name_utils.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QFileInfo>
#include <QHeaderView>
#include <QRegularExpression>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QString>
#include <QTreeView>
#include <QVBoxLayout>
#include <QVariant>

#include <libremidi/libremidi.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Protocols::MCUSettingsWidget)

namespace Protocols
{
namespace
{
enum InstrumentRole
{
  //! "Moog Matriarch" on a device row, "Moog" on a manufacturer row, so that
  //! either half of the name matches.
  SearchRole = Qt::UserRole + 1,
  //! A device map's identity, "ardour/donnerdmk25.midimap.json". Empty on a
  //! MIDI Guide row, which is named by manufacturer and device instead.
  MapRole
};

//! Index into the port vectors, or -1 for "(none)".
int portIndex(const QComboBox& combo)
{
  const auto data = combo.currentData();
  return data.isValid() ? data.toInt() : -1;
}

//! False when the live ports do not include it, which getSettings must not
//! read as "the user chose none".
bool selectPort(QComboBox& combo, const QString& displayName)
{
  if(displayName.isEmpty())
    return false;

  for(int i = 0; i < combo.count(); i++)
  {
    if(combo.itemText(i) == displayName)
    {
      combo.setCurrentIndex(i);
      return true;
    }
  }
  return false;
}

QString portName(const libremidi::port_information& p)
{
  return QString::fromStdString(
      p.display_name.empty() ? p.port_name : p.display_name);
}

//! Position of @p wanted in @p ports, or -1. Identity is the handle plus the
//! names: the handle alone repeats across backends, and the names alone repeat
//! across two identical interfaces.
template <typename Port>
int indexOfPort(const std::vector<Port>& ports, const Port& wanted)
{
  for(std::size_t i = 0; i < ports.size(); i++)
  {
    const auto& p = ports[i];
    if(p.port == wanted.port && p.port_name == wanted.port_name
       && p.device_name == wanted.device_name)
      return int(i);
  }
  return -1;
}
/**
 * A node-address name for a piece of hardware, from what a driver calls one of
 * its ports.
 *
 * A port name carries three things an address should not: which of the device's
 * ports this is ("MIDI 1", "Port-0"), the brand, and punctuation. `M-Audio
 * Keystation Pro 88 MIDI 1` is `KeystationPro88`.
 */
QString hardwareName(QString raw, const QString& manufacturer)
{
  static const QRegularExpression tail{
      R"([ _-]*(MIDI|Port)[ _-]*\d*$)", QRegularExpression::CaseInsensitiveOption};
  raw.remove(tail);

  if(!manufacturer.isEmpty() && raw.startsWith(manufacturer, Qt::CaseInsensitive))
    raw = raw.mid(manufacturer.size());

  // A brand the port names but the manufacturer field does not.
  static const QRegularExpression brand{
      R"(^\s*(M[ _-]?Audio|Akai( Professional)?|Arturia|Novation|Korg|Roland|Yamaha|Behringer|Native Instruments|Focusrite|PreSonus|Nektar|Alesis|Icon|Steinberg|Denon|Numark|Pioneer( DJ)?|Hercules|Elektron|Moog|Teenage Engineering)\b)",
      QRegularExpression::CaseInsensitiveOption};
  raw.remove(brand);

  raw = raw.trimmed();
  ossia::net::sanitize_name(raw);
  return raw;
}

//! Is this port currently offered by @p combo? The port vector keeps entries
//! whose row has been removed, so it cannot answer this.
bool isListed(const QComboBox& combo, int portIdx)
{
  for(int row = 0; row < combo.count(); row++)
    if(combo.itemData(row).isValid() && combo.itemData(row).toInt() == portIdx)
      return true;
  return false;
}
}

MCUSettingsWidget::MCUSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_name = new State::AddressFragmentLineEdit{this};
  checkForChanges(m_name);

  m_kind = new QComboBox{this};
  m_kind->addItem(tr("Mackie Control surface"), int(MCUSpecificSettings::MCU));
  m_kind->addItem(
      tr("Controller or instrument (device map)"),
      int(MCUSpecificSettings::MidiDeviceMap));

  // The device map covers both kinds of hardware and several hundred devices;
  // a Mackie surface is the special case.
  m_kind->setCurrentIndex(
      m_kind->findData(int(MCUSpecificSettings::MidiDeviceMap)));
  checkForChanges(m_kind);

  m_midiin = new QComboBox{this};
  m_midiout = new QComboBox{this};

  // An instrument is usable one-way, so neither port is mandatory.
  m_midiin->addItem(tr("(none)"), -1);
  m_midiout->addItem(tr("(none)"), -1);
  checkForChanges(m_midiin);
  checkForChanges(m_midiout);

  auto lay = new QFormLayout;
  lay->addRow(tr("Name"), m_name);
  lay->addRow(tr("Controller"), m_kind);
  lay->addRow(tr("MIDI input"), m_midiin);
  lay->addRow(tr("MIDI output"), m_midiout);

  /// The instrument picker ///
  m_instrumentBox = new QWidget{this};
  {
    auto box = new score::MarginLess<QVBoxLayout>{m_instrumentBox};

    m_search = new QLineEdit{m_instrumentBox};
    m_search->setPlaceholderText(tr("Search an instrument..."));
    m_search->setClearButtonEnabled(true);
    box->addWidget(m_search);

    m_instrumentModel = new QStandardItemModel{m_instrumentBox};
    m_instrumentModel->setHorizontalHeaderLabels({tr("Device"), tr("Configuration")});

    m_instrumentFilter = new QSortFilterProxyModel{m_instrumentBox};
    m_instrumentFilter->setSourceModel(m_instrumentModel);
    m_instrumentFilter->setFilterRole(SearchRole);
    m_instrumentFilter->setFilterCaseSensitivity(Qt::CaseInsensitive);
    // So that a manufacturer stays in the tree while one of its instruments
    // matches: the search is on the leaves.
    m_instrumentFilter->setRecursiveFilteringEnabled(true);

    m_instruments = new QTreeView{m_instrumentBox};
    m_instruments->setModel(m_instrumentFilter);

    // The header is what the user drags to widen the name column, so it has to
    // be visible; with two columns it also says which is which.
    auto* header = m_instruments->header();
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setStretchLastSection(true);
    header->setMinimumSectionSize(60);
    m_instruments->setHeaderHidden(false);
    m_instruments->setUniformRowHeights(true);
    m_instruments->setAllColumnsShowFocus(true);
    m_instruments->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_instruments->setSelectionMode(QAbstractItemView::SingleSelection);
    m_instruments->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_instruments->setMinimumHeight(160);
    box->addWidget(m_instruments, 1);

    auto sub = new QFormLayout;
    m_channel = new QSpinBox{m_instrumentBox};
    m_channel->setRange(1, 16);
    m_channel->setToolTip(
        tr("The MIDI channel the instrument is set to. The dataset documents "
           "parameter numbers without a channel, so this is the one thing it "
           "cannot tell us."));
    checkForChanges(m_channel);
    sub->addRow(tr("Channel"), m_channel);
    box->addLayout(sub);

    m_summary = new QLabel{m_instrumentBox};
    m_summary->setWordWrap(true);
    box->addWidget(m_summary);
  }
  lay->addRow(m_instrumentBox);

  connect(m_search, &QLineEdit::textChanged, this, [this](const QString& text) {
    m_instrumentFilter->setFilterFixedString(text);
    // A filtered tree is only useful expanded, an unfiltered one collapsed.
    if(text.isEmpty())
      m_instruments->collapseAll();
    else
      m_instruments->expandAll();
  });

  connect(
      m_instruments->selectionModel(), &QItemSelectionModel::selectionChanged, this,
      [this] {
    if(const auto live = selectedMap(); !live.isEmpty())
      m_chosenMap = live;
    refreshAutoName();
    updateDeviceMapSummary();
    changed();
  });

  connect(
      m_kind, qOverload<int>(&QComboBox::currentIndexChanged), this,
      &MCUSettingsWidget::updateKind);

  // The device is named after the hardware it talks to, until the user names it
  // themselves.
  for(auto* combo : {m_midiin, m_midiout})
    connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &MCUSettingsWidget::refreshAutoName);

  // Enumerated with the API the settings will name: a port handle only means
  // something to the backend that produced it.
  libremidi::observer_configuration conf;
  conf.input_added = [this](const libremidi::input_port& p) {
    QMetaObject::invokeMethod(this, [this, p] { addInput(p); });
  };
  conf.output_added = [this](const libremidi::output_port& p) {
    QMetaObject::invokeMethod(this, [this, p] { addOutput(p); });
  };
  conf.input_removed = [this](const libremidi::input_port& p) {
    QMetaObject::invokeMethod(this, [this, p] { removePort(*m_midiin, m_ins, p); });
  };
  conf.output_removed = [this](const libremidi::output_port& p) {
    QMetaObject::invokeMethod(this, [this, p] { removePort(*m_midiout, m_outs, p); });
  };

  /*
   * The initial list is queried, not taken from notify_in_constructor: the
   * PipeWire observer stops reporting through its callbacks once any PipeWire
   * port has been opened and closed in the process, while get_input_ports()
   * keeps answering. Querying is synchronous too, so the lists are filled
   * before the first paint. The callbacks remain for hotplug.
   */
  conf.notify_in_constructor = false;
  // Every transport group: a network peer is as valid a choice as a USB one.
  conf.track_hardware = true;
  conf.track_virtual = true;
  conf.track_network = true;
  m_api = getCurrentAPI();
  m_observer = std::make_unique<libremidi::observer>(
      conf, libremidi::observer_configuration_for(m_api));

  for(const auto& p : m_observer->get_input_ports())
    addInput(p);
  for(const auto& p : m_observer->get_output_ports())
    addOutput(p);

  setLayout(lay);

  populateDeviceMaps();
  updateKind();
}

MCUSettingsWidget::~MCUSettingsWidget() { }

/**
 * The same port can arrive twice, from the initial query and from a callback
 * already in flight. Item data is the index into the port vector, never the
 * combo row, so removing a row cannot repoint the others.
 */
void MCUSettingsWidget::addInput(const libremidi::input_port& p)
{
  if(const int known = indexOfPort(m_ins, p); known != -1)
  {
    if(!isListed(*m_midiin, known))
      m_midiin->addItem(portName(p), known);
    return;
  }

  m_ins.push_back(p);
  m_midiin->addItem(portName(p), int(m_ins.size()) - 1);
}

void MCUSettingsWidget::addOutput(const libremidi::output_port& p)
{
  if(const int known = indexOfPort(m_outs, p); known != -1)
  {
    if(!isListed(*m_midiout, known))
      m_midiout->addItem(portName(p), known);
    return;
  }

  m_outs.push_back(p);
  m_midiout->addItem(portName(p), int(m_outs.size()) - 1);
}

/**
 * The port vector is deliberately left alone: other rows hold indices into it
 * and compacting it would repoint them. @see isListed
 */
template <typename Port>
void MCUSettingsWidget::removePort(
    QComboBox& combo, const std::vector<Port>& ports, const Port& gone)
{
  const int idx = indexOfPort(ports, gone);
  if(idx == -1)
    return;

  for(int row = 0; row < combo.count(); row++)
  {
    if(combo.itemData(row).isValid() && combo.itemData(row).toInt() == idx)
    {
      combo.removeItem(row);
      return;
    }
  }
}


/**
 * Name the device after what it talks to: the hardware a chosen description
 * names, and the port otherwise.
 *
 * The description wins because it names one piece of hardware, while a port
 * may carry several devices on different channels and its name would then
 * describe none of them in particular.
 */
void MCUSettingsWidget::refreshAutoName()
{
  // The model, not the label: the preset belongs in the picker, not in every
  // address under the device.
  if(const auto* e = MIDIDevices::Database::instance().find(chosenMap()))
  {
    applyAutoName(hardwareName(
        QString::fromStdString(e->header.model),
        QString::fromStdString(e->header.manufacturer)));
    return;
  }

  applyAutoName(nameFromPorts());
}

//! What MCUProtocolFactory::defaultSettings() calls a new device.
QString MCUSettingsWidget::defaultName()
{
  return QStringLiteral("MCU");
}

QString MCUSettingsWidget::nameFromPorts() const
{
  // The input first: a controller is a controller because it sends.
  if(const int in = portIndex(*m_midiin); in >= 0 && in < std::ssize(m_ins))
  {
    const auto& p = m_ins[in];
    return hardwareName(
        QString::fromStdString(p.device_name.empty() ? p.port_name : p.device_name),
        QString::fromStdString(p.manufacturer));
  }
  if(const int out = portIndex(*m_midiout); out >= 0 && out < std::ssize(m_outs))
  {
    const auto& p = m_outs[out];
    return hardwareName(
        QString::fromStdString(p.device_name.empty() ? p.port_name : p.device_name),
        QString::fromStdString(p.manufacturer));
  }
  return {};
}

void MCUSettingsWidget::applyAutoName(const QString& name)
{
  if(name.isEmpty())
    return;

  // The name the factory fills in is a placeholder, not a choice: without this
  // the device would keep it forever, since it is neither empty nor ours.
  const auto current = m_name->text();
  const bool userNamed
      = !current.isEmpty() && current != m_autoName && current != defaultName();
  if(userNamed)
    return;

  m_autoName = name;
  m_name->setText(name);
}

/**
 * Grouped by manufacturer, and by the source the map was converted from where
 * the manufacturer is unknown: a Cubase Generic Remote export names only the
 * user's own control labels, and several hundred rows called "(unnamed)" would
 * be worse than one group saying where they came from.
 */
void MCUSettingsWidget::populateDeviceMaps()
{
  m_instrumentModel->clear();
  m_instrumentModel->setHorizontalHeaderLabels({tr("Device"), tr("Configuration")});

  // clear() takes the columns and their widths with it.
  m_instruments->header()->resizeSection(0, 320);

  QStandardItem* groupItem{};
  QString currentGroup;

  // Already sorted by manufacturer, then model, then preset.
  for(const auto& entry : MIDIDevices::Database::instance().devices())
  {
    auto group = QString::fromStdString(entry.header.manufacturer);
    if(group.isEmpty())
      group = tr("Unnamed (%1)").arg(entry.source);

    if(!groupItem || group != currentGroup)
    {
      currentGroup = group;
      groupItem = new QStandardItem{group};
      groupItem->setData(group, SearchRole);
      groupItem->setSelectable(false);
      m_instrumentModel->appendRow(groupItem);
    }

    auto* item = new QStandardItem{entry.label()};
    item->setData(QString{group + " " + entry.label() + " " + entry.source}, SearchRole);
    item->setData(entry.identity, MapRole);

    /*
     * What tells two documents of the same device apart. Several sources ship
     * one model under many configurations -- the E-mu P2000 has eighteen
     * documents all calling themselves "Audity 2000", one per ROM card, and
     * none of them states a preset -- so without this the picker offers
     * eighteen identical rows.
     */
    auto detail = QString::fromStdString(entry.header.description);
    if(detail.isEmpty())
      detail = QFileInfo{entry.file}.fileName();
    detail.remove(QLatin1String{".midimap.json"});
    groupItem->appendRow({item, new QStandardItem{detail}});
  }
}

QString MCUSettingsWidget::selectedMap() const
{
  const auto rows = m_instruments->selectionModel()->selectedRows();
  if(rows.empty())
    return {};
  return rows.front().data(MapRole).toString();
}

/**
 * The map the user picked, which is not the same as the row the view happens to
 * be showing: typing in the search box filters the selected row out and clears
 * the selection, and that must not read as unpicking the device.
 */
QString MCUSettingsWidget::chosenMap() const
{
  if(const auto live = selectedMap(); !live.isEmpty())
    return live;
  return m_chosenMap;
}

void MCUSettingsWidget::selectMap(const QString& identity)
{
  m_chosenMap = identity;
  m_instruments->setCurrentIndex(QModelIndex{});
  m_instruments->clearSelection();

  if(identity.isEmpty())
    return;

  for(int g = 0; g < m_instrumentModel->rowCount(); g++)
  {
    auto* groupItem = m_instrumentModel->item(g);
    for(int d = 0; d < groupItem->rowCount(); d++)
    {
      auto* item = groupItem->child(d);
      if(item->data(MapRole).toString() != identity)
        continue;

      if(const auto idx = m_instrumentFilter->mapFromSource(item->index());
         idx.isValid())
      {
        m_instruments->setCurrentIndex(idx);
        m_instruments->scrollTo(idx);
      }
      return;
    }
  }
}

void MCUSettingsWidget::updateDeviceMapSummary()
{
  auto& db = MIDIDevices::Database::instance();

  const auto identity = chosenMap();
  if(identity.isEmpty())
  {
    if(db.devices().empty())
      m_summary->setText(
          tr("No device map found. Install the \"MIDI device maps\" package from "
             "the package manager to get the control maps of several hundred "
             "controllers and instruments."));
    else
      m_summary->setText(tr("Select a device above."));
    return;
  }

  const auto* entry = db.find(identity);
  if(!entry)
  {
    m_summary->setText(tr("%1 is no longer in the library.").arg(identity));
    return;
  }

  const auto map = MIDIDevices::Database::load(*entry);
  if(!map)
  {
    m_summary->setText(tr("%1 could not be read.").arg(identity));
    return;
  }

  auto text = tr("%n control(s).", "", int(map->controls.size()));

  // A map is only true for the configuration it describes, so anything the
  // description asks of the user belongs in front of them before they connect.
  if(!map->preset.name.empty())
    text += " " + tr("Set the device to: %1.")
                      .arg(QString::fromStdString(map->preset.name));
  if(!map->requirement.empty())
    text += " " + QString::fromStdString(map->requirement);

  m_summary->setText(text);
}

void MCUSettingsWidget::updateKind()
{
  const auto mode
      = static_cast<MCUSpecificSettings::Mode>(m_kind->currentData().toInt());
  const bool picks = mode != MCUSpecificSettings::MCU;

  m_instrumentBox->setVisible(picks);
  if(!picks)
    return;

  // The two kinds fill the same tree from different libraries, so switching
  // between them has to refill it.
  if(mode != m_populated)
  {
    m_populated = mode;
    populateDeviceMaps();
  }
  updateDeviceMapSummary();
}

Device::DeviceSettings MCUSettingsWidget::getSettings() const
{
  Device::DeviceSettings s = m_current;
  MCUSpecificSettings midi = s.deviceSpecificSettings.value<MCUSpecificSettings>();
  s.name = m_name->text();
  s.protocol = MCUProtocolFactory::static_concreteKey();

  midi.mode
      = static_cast<MCUSpecificSettings::Mode>(m_kind->currentData().toInt());

  // The API that produced the handles below. Stored rather than re-derived on
  // connection: the user may change the setting afterwards, and the handles
  // would then belong to the wrong backend.
  midi.api = m_api;

  /*
   * A port is only overwritten when a row names one. setSettings can only
   * select a row when a live port matches what was saved, so an unplugged
   * interface leaves the combo on "(none)" -- and erasing the handle there
   * would leave a device that checkCompatibility refuses, with no way back.
   * Choosing "(none)" deliberately is the only case that should erase.
   */
  if(const int in = portIndex(*m_midiin); in >= 0 && in < std::ssize(m_ins))
    midi.input_handle = {m_ins[in]};
  else if(m_midiin->currentIndex() == 0 && m_portsResolved)
    midi.input_handle.clear();

  if(const int out = portIndex(*m_midiout); out >= 0 && out < std::ssize(m_outs))
    midi.output_handle = {m_outs[out]};
  else if(m_midiout->currentIndex() == 0 && m_portsResolved)
    midi.output_handle.clear();

  // Likewise the instrument: a missing package must not repoint the device.
  if(const auto identity = chosenMap(); !identity.isEmpty())
    midi.map = identity;
  midi.channel = m_channel->value();

  s.deviceSpecificSettings = QVariant::fromValue(midi);

  return s;
}

void MCUSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_current = settings;
  const auto& s = m_current.deviceSpecificSettings.value<MCUSpecificSettings>();

  // Clean up the name a bit
  auto pretty_name = settings.name;
  if(!pretty_name.isEmpty())
  {
    pretty_name = pretty_name.split(':').front();
    ossia::net::sanitize_device_name(pretty_name);
  }

  m_name->setText(pretty_name);

  if(const int idx = m_kind->findData(int(s.mode)); idx >= 0)
    m_kind->setCurrentIndex(idx);

  // Matched by the name the user saw: a USB interface's handle changes between
  // sessions. The lists are already filled, so a name absent from them means
  // the port is absent.

  m_portsResolved = true;
  if(!s.input_handle.empty())
    m_portsResolved &= selectPort(*m_midiin, portName(s.input_handle.front()));
  if(!s.output_handle.empty())
    m_portsResolved &= selectPort(*m_midiout, portName(s.output_handle.front()));

  m_channel->setValue(std::clamp(s.channel, 1, 16));

  // Before the selection: the tree has to hold the right library first.
  updateKind();

  selectMap(s.map);

  updateDeviceMapSummary();
}
}
