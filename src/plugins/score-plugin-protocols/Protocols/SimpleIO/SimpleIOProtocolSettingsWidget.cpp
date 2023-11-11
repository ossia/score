#include "libsimpleio/libgpio.h"
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_SIMPLEIO)
#include "SimpleIOProtocolFactory.hpp"
#include "SimpleIOProtocolSettingsWidget.hpp"
#include "SimpleIOSpecificSettings.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Library/LibrarySettings.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/model/tree/TreeNodeItemModel.hpp>
#include <score/tools/FindStringInFile.hpp>
#include <score/tools/ListNetworkAddresses.hpp>

#include <ossia/detail/flat_map.hpp>
#include <ossia/detail/hash_map.hpp>
#include <ossia/detail/math.hpp>
#include <ossia/detail/string_algorithms.hpp>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDirIterator>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QTableWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QVariant>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Protocols::SimpleIOProtocolSettingsWidget)

namespace Protocols
{


struct SimpleIOData
{
  QString name;
  QString path;

};

using SimpleIONode = TreeNode<SimpleIOData>;


class SimpleIODatabase : public TreeNodeBasedItemModel<SimpleIONode>
{
public:
  SimpleIODatabase()
  {
    beginResetModel();
    auto& pwms = m_root.emplace_back(SimpleIOData{.name = "PWMs"}, &m_root);
    auto& adcs = m_root.emplace_back(SimpleIOData{.name = "ADCs"}, &m_root);
    auto& dacs = m_root.emplace_back(SimpleIOData{.name = "DACs"}, &m_root);
    auto& gpios = m_root.emplace_back(SimpleIOData{.name = "GPIOs"}, &m_root);
    auto& hid = m_root.emplace_back(SimpleIOData{.name = "HIDs"}, &m_root);

    enumeratePWMs(pwms);
    enumerateADCs(adcs);
    enumerateDACs(dacs);
    enumerateGPIOs(gpios);
    enumerateHIDs(gpios);

    // List ADCs

    // List DACs

    endResetModel();

  }

  void enumeratePWMs(SimpleIONode& n) {
    QDirIterator dir{"/sys/class/pwm",
                     QDir::Dirs | QDir::NoDotAndDotDot,
                     QDirIterator::NoIteratorFlags};

    while(dir.hasNext()) {
      auto pwm = dir.nextFileInfo();
      n.emplace_back(SimpleIOData{.name = pwm.fileName(), .path = pwm.absoluteFilePath()}, &m_root);
    }
  }

  void enumerateADCs(SimpleIONode& n) {

  }

  void enumerateDACs(SimpleIONode& n) {

  }

  void enumerateHIDs(SimpleIONode& n) {
    for(int i = 0; i < 128; i++) {
      if(auto dev = QString{"/dev/hidraw%1"}.arg(i); QFile::exists(dev)) {
        n.emplace_back(SimpleIOData{.name = QString{"hidraw%1"}.arg(i), .path = dev}, &m_root);
      }
      else
      {
        break;
      }
    }
  }

  void enumerateGPIOs(SimpleIONode& n) {
    for(int chip = 0; chip < 128; chip++) {
      char name[256];
      char label[256];
      int32_t lines, error;
      GPIO_chip_info(chip, name, 255, label, 255, &lines, &error);

      if(error != 0)
        break;
      qDebug() << "GPIO Chip " << chip << " : " << name << label << lines;
      auto& chip_node = n.emplace_back(SimpleIOData{.name = QString{"GPIO chip %1"}.arg(chip), .path = name}, &n);
      {
        for(int line = 0; line < lines; line++)
        {
          int32_t flags;
          GPIO_line_info(chip, line, &flags, name, 255, label, 255, &error);
          qDebug() << " - Line " << line << " : " << name << label << lines;
          chip_node.emplace_back(SimpleIOData{.name = QString{"%1: %2"}.arg(chip).arg(name), .path = QString{"%1:%2"}.arg(chip).arg(line)}, &chip_node);
        }
      }
    }
  }


  SimpleIONode& rootNode() override { return m_root; }
  const SimpleIONode& rootNode() const override { return m_root; }

  int columnCount(const QModelIndex& parent) const override { return 2; }

  QVariant data(const QModelIndex& index, int role) const override
  {
    const auto& node = nodeFromModelIndex(index);
    if(index.column() == 0)
    {
      switch(role)
      {
        case Qt::DisplayRole:
          return node.name;
          //        case Qt::DecorationRole:
          //          return node.icon;
      }
    }
    return QVariant{};
  }

  QVariant headerData(int section, Qt::Orientation orientation, int role) const override
  {
    if(role != Qt::DisplayRole)
      return TreeNodeBasedItemModel<SimpleIONode>::headerData(section, orientation, role);
    switch(section)
    {
      case 0:
        return tr("Name");
      case 1:
        return tr("Path");
      default:
        return {};
    }
  }
  Qt::ItemFlags flags(const QModelIndex& index) const override
  {
    Qt::ItemFlags f;

    f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;

    return f;
  }

  static SimpleIODatabase& instance()
  {
    static SimpleIODatabase db;
    return db;
  }

  SimpleIONode m_root;
};

class SimpleIOTreeView : public QTreeView
{
public:
  SimpleIOTreeView(QWidget* parent = nullptr)
      : QTreeView{parent}
  {
    setAllColumnsShowFocus(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDragDropMode(QAbstractItemView::NoDragDrop);
    setAlternatingRowColors(true);
    setUniformRowHeights(true);
  }

  std::function<void(const SimpleIONode&)> onSelectionChanged;
  void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
  {
    auto sel = this->selectedIndexes();
    if(!sel.empty())
    {
      auto obj = (SimpleIONode*)sel.at(0).internalPointer();
      if(obj)
      {
        onSelectionChanged(*obj);
      }
    }
  }
};



class AddPortDialog : public QDialog
{
public:
  AddPortDialog(SimpleIOProtocolSettingsWidget& parent)
      : QDialog{&parent}
      , m_name{this}
      , m_buttons{
            QDialogButtonBox::StandardButton::Ok
                | QDialogButtonBox::StandardButton::Cancel,
            this}
  {
    this->setLayout(&m_layout);
    m_layout.addWidget(&m_availablePorts);
    m_layout.addLayout(&m_setupLayoutContainer);
    m_layout.setStretch(0, 3);
    m_layout.setStretch(1, 5);

    m_availablePorts.setModel(&SimpleIODatabase::instance());
    m_availablePorts.header()->resizeSection(0, 180);
    m_availablePorts.onSelectionChanged = [&](const SimpleIONode& newFixt) {
      // Manufacturer, do nothing
      if(newFixt.childCount() > 0)
        return;

      updateParameters(newFixt);
    };

    m_address.setRange(1, 512);

    m_setupLayoutContainer.addLayout(&m_setupLayout);
    m_setupLayout.addRow(tr("Name"), &m_name);
    m_setupLayout.addRow(tr("Address"), &m_address);
    m_setupLayout.addRow(tr("Mode"), &m_mode);
    m_setupLayout.addRow(tr("Channels"), &m_content);
    m_setupLayoutContainer.addStretch(0);
    m_setupLayoutContainer.addWidget(&m_buttons);
    connect(&m_buttons, &QDialogButtonBox::accepted, this, &AddPortDialog::accept);
    connect(&m_buttons, &QDialogButtonBox::rejected, this, &AddPortDialog::reject);

    connect(
        &m_mode, qOverload<int>(&QComboBox::currentIndexChanged), this,
        &AddPortDialog::setMode);
  }

  void updateParameters(const SimpleIONode& fixt)
  {/*
    m_name.setText(fixt.name);

    m_mode.clear();
    for(auto& mode : fixt.modes)
      m_mode.addItem(mode.name);
    m_mode.setCurrentIndex(0);

    m_currentPort = &fixt;

    setMode(0);*/
  }

  void setMode(int mode_index)
  {/*
    if(!m_currentPort)
      return;
    if(!ossia::valid_index(mode_index, m_currentPort->modes))
      return;

    const PortMode& mode = m_currentPort->modes[mode_index];
    int numChannels = mode.allChannels.size();
    m_address.setRange(1, 513 - numChannels);

    m_content.setText(mode.content());*/
  }

  QSize sizeHint() const override { return QSize{800, 600}; }

  SimpleIO::Port port() const noexcept
  {
    SimpleIO::Port f;
    /*
    if(!m_currentPort)
      return f;

    int mode_index = m_mode.currentIndex();
    if(!ossia::valid_index(mode_index, m_currentPort->modes))
      return f;

    auto& mode = m_currentPort->modes[mode_index];

    f.portName = m_currentPort->name;
    f.modeName = m_mode.currentText();
    f.mode.channelNames = mode.allChannels;
    f.address = m_address.value() - 1;
    f.controls = mode.channels;
*/
    return f;
  }

private:
  QHBoxLayout m_layout;
  SimpleIOTreeView m_availablePorts;

  QVBoxLayout m_setupLayoutContainer;
  QFormLayout m_setupLayout;
  State::AddressFragmentLineEdit m_name;
  QSpinBox m_address;
  QComboBox m_mode;
  QLabel m_content;
  QDialogButtonBox m_buttons;
};

SimpleIOProtocolSettingsWidget::SimpleIOProtocolSettingsWidget(QWidget* parent)
    : Device::ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  m_deviceNameEdit->setText("SimpleIO");

  auto layout = new QFormLayout;
  layout->addRow(tr("Name"), m_deviceNameEdit);

  m_portsWidget = new QTableWidget;
  layout->addRow(m_portsWidget);

  m_portsWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_portsWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_portsWidget->insertColumn(0);
  m_portsWidget->insertColumn(1);
  m_portsWidget->insertColumn(2);
  m_portsWidget->setHorizontalHeaderLabels({tr("Name"), tr("Type"), tr("Mode")});

  auto btns = new QHBoxLayout;
  m_addPort = new QPushButton{"Add a port"};
  m_rmPort = new QPushButton{"Remove selected port"};
  btns->addWidget(m_addPort);
  btns->addWidget(m_rmPort);
  layout->addRow(btns);

  connect(m_addPort, &QPushButton::clicked, this, [this] {

#if 0
    auto dial = new AddPortDialog{*this};
    if(dial->exec() == QDialog::Accepted)
    {
      auto port = dial->port();
      if(!port.name.isEmpty() && !port.controls.empty())
      {
        m_ports.push_back(port);
        updateTable();
      }
    }
#endif
  });
  connect(m_rmPort, &QPushButton::clicked, this, [this] {
    ossia::flat_set<int> rows_to_remove;
    for(auto item : m_portsWidget->selectedItems())
    {
      rows_to_remove.insert(item->row());
    }

    for(auto it = rows_to_remove.rbegin(); it != rows_to_remove.rend(); ++it)
    {
      m_ports.erase(m_ports.begin() + *it);
    }

    updateTable();
  });

  setLayout(layout);
}

void SimpleIOProtocolSettingsWidget::updateTable()
{
  while(m_portsWidget->rowCount() > 0)
    m_portsWidget->removeRow(int(m_portsWidget->rowCount()) - 1);

  int row = 0;
  for(auto& fixt : m_ports)
  {
#if 0    
    auto name_item = new QTableWidgetItem{fixt.portName};
    auto mode_item = new QTableWidgetItem{fixt.modeName};
    auto address = new QTableWidgetItem{QString::number(fixt.address + 1)};
    auto controls = new QTableWidgetItem{QString::number(fixt.controls.size())};
    name_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    mode_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    address->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    controls->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

    m_portsWidget->insertRow(row);
    m_portsWidget->setItem(row, 0, name_item);
    m_portsWidget->setItem(row, 1, mode_item);
    m_portsWidget->setItem(row, 2, address);
    m_portsWidget->setItem(row, 3, controls);
    row++;
#endif
  }
}
SimpleIOProtocolSettingsWidget::~SimpleIOProtocolSettingsWidget() { }

Device::DeviceSettings SimpleIOProtocolSettingsWidget::getSettings() const
{
  // TODO should be = m_settings to follow the other patterns.
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = SimpleIOProtocolFactory::static_concreteKey();

  SimpleIOSpecificSettings settings{};
  settings.ports = this->m_ports;
  s.deviceSpecificSettings = QVariant::fromValue(settings);

  return s;
}

void SimpleIOProtocolSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
  const auto& specif = settings.deviceSpecificSettings.value<SimpleIOSpecificSettings>();
  m_ports = specif.ports;
  updateTable();
}
}
#endif
