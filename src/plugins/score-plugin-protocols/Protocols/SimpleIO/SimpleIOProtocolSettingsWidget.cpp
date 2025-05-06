#include "libsimpleio/libgpio.h"

#include <score/tools/File.hpp>

#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_SIMPLEIO)
#include "SimpleIOProtocolFactory.hpp"
#include "SimpleIOProtocolSettingsWidget.hpp"
#include "SimpleIOSpecificSettings.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Library/LibrarySettings.hpp>
#include <Protocols/OSC/OSCProtocolSettingsWidget.hpp>
#include <Protocols/SimpleIO/Wokwi/Layout.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/model/tree/TreeNodeItemModel.hpp>

#include <ossia/detail/flat_map.hpp>
#include <ossia/detail/hash_map.hpp>
#include <ossia/detail/math.hpp>
#include <ossia/detail/string_algorithms.hpp>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDirIterator>
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStackedLayout>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QUrl>
#include <QVariant>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Protocols::SimpleIOProtocolSettingsWidget)

namespace Protocols
{
struct SimpleIOData : SimpleIO::Port
{
  QString tree_name{};
  QString label{};
};

using SimpleIONode = TreeNode<SimpleIOData>;
class SimpleIODatabase : public TreeNodeBasedItemModel<SimpleIONode>
{
public:
  SimpleIODatabase()
  {
    beginResetModel();
    auto& pwms = m_root.emplace_back(SimpleIOData{{.name = "PWMs"}}, &m_root);
    auto& adcs = m_root.emplace_back(SimpleIOData{{.name = "ADCs"}}, &m_root);
    auto& dacs = m_root.emplace_back(SimpleIOData{{.name = "DACs"}}, &m_root);
    auto& gpios = m_root.emplace_back(SimpleIOData{{.name = "GPIOs"}}, &m_root);
    //    auto& hid = m_root.emplace_back(SimpleIOData{.name = "HIDs"}, &m_root);

    pwms.emplace_back(
        SimpleIOData{
            {.control = SimpleIO::PWM{}, .name = "external", .path = ""},
            "External PWM",
            ""},
        &pwms);
    adcs.emplace_back(
        SimpleIOData{
            {.control = SimpleIO::ADC{}, .name = "external", .path = ""},
            "External ADC",
            ""},
        &adcs);
    dacs.emplace_back(
        SimpleIOData{
            {.control = SimpleIO::DAC{}, .name = "external", .path = ""},
            "External DAC",
            ""},
        &dacs);
    gpios.emplace_back(
        SimpleIOData{
            {.control = SimpleIO::GPIO{}, .name = "external", .path = ""},
            "External GPIO",
            ""},
        &gpios);

    enumeratePWMs(pwms);
    enumerateADCs(adcs, dacs);
    enumerateGPIOs(gpios);
//    enumerateHIDs(hid);

    // List ADCs

    // List DACs

    endResetModel();

  }

  void enumeratePWMs(SimpleIONode& n) {
    QDirIterator dir{
        "/sys/class/pwm", QDir::Dirs | QDir::NoDotAndDotDot,
        QDirIterator::NoIteratorFlags};

    int i = 0;
    while(dir.hasNext()) {
      auto pwm = QFileInfo{dir.next()};
      n.emplace_back(
          SimpleIOData{
              {
                  .control = SimpleIO::PWM{},
                  .name = QString{"PWM %1"}.arg(i++),
                  .path = pwm.absoluteFilePath(),
              },
              {},
              pwm.fileName()},
          &n);
    }
  }

  void enumerateADCs(SimpleIONode& adc_n, SimpleIONode& dac_n)
  {
    QDirIterator dir{
        "/sys/bus/iio/devices/", QDir::Dirs | QDir::NoDotAndDotDot,
        QDirIterator::NoIteratorFlags};

    int adc_i = 0;
    int dac_i = 0;
    while(dir.hasNext())
    {
      const auto iio = QFileInfo{dir.next()};
      if(QString name = iio.fileName(); name.startsWith("iio:device"))
      {
        name.remove("iio:device");
        bool ok = false;
        int idx = name.toInt(&ok);
        if(!ok)
          continue;

        QDirIterator iio_it(iio.dir());
        while(iio_it.hasNext())
        {
          const auto voltage = QFileInfo{iio_it.next()};
          auto vname = voltage.fileName();
          if(vname.startsWith("in_voltage") && vname.endsWith("_raw"))
          {
            vname.remove("in_voltage");
            vname.remove("_raw");
            ok = false;
            const int channel = vname.toInt(&ok);
            if(!ok)
              continue;

            adc_n.emplace_back(
                SimpleIOData{
                    {
                        .control = SimpleIO::ADC{.chip = idx, .channel = channel},
                        .name = QString{"ADC %1"}.arg(adc_i++),
                        .path = voltage.absoluteFilePath(),
                    },
                    {},
                    voltage.fileName()},
                &adc_n);
          }
          else if(vname.startsWith("out_voltage") && vname.endsWith("_raw"))
          {
            vname.remove("out_voltage");
            vname.remove("_raw");
            ok = false;
            const int channel = vname.toInt(&ok);
            if(!ok)
              continue;

            dac_n.emplace_back(
                SimpleIOData{
                    {
                        .control = SimpleIO::DAC{.chip = idx, .channel = channel},
                        .name = QString{"DAC %1"}.arg(dac_i++),
                        .path = voltage.absoluteFilePath(),
                    },
                    {},
                    voltage.fileName()},
                &dac_n);
          }
        }
      }
    };
  }

  void enumerateHIDs(SimpleIONode& n) {
    for(int i = 0; i < 128; i++) {
      if(const auto dev = QString{"/dev/hidraw%1"}.arg(i); QFile::exists(dev))
      {
        n.emplace_back(
            SimpleIOData{
                {
                    .control = SimpleIO::HID{},
                    .name = QString{"HID Raw %1"}.arg(i),
                    .path = dev,
                },
                {},
                QString{"hidraw%1"}.arg(i)},
            &n);
      }
      else
      {
        break;
      }
    }
  }

  void enumerateGPIOs(SimpleIONode& n) {
    for(int chip = 0; chip < 128; chip++) {
      char name[256]{};
      char label[256]{};
      int32_t lines{}, error{};
      GPIO_chip_info(chip, name, 255, label, 255, &lines, &error);

      QString s_chip = QString::number(chip);
      if(error != 0)
        break;
      auto& chip_node = n.emplace_back(
          SimpleIOData{{.name = QString{"GPIO chip %1"}.arg(chip)}}, &n);
      {
        for(int line = 0; line < lines; line++)
        {
          int32_t flags;
          GPIO_line_info(chip, line, &flags, name, 255, label, 255, &error);
          if(error != 0)
            break;

          QString s_name = name;
          QString s_label = label;
          QString s_line = QString::number(line);
          QString pretty_name;      // name
          QString pretty_tree_name; // QString{"%1: %2"}.arg(line).arg(name)
          QString pretty_label;     // QString{"%1: %2"}.arg(line).arg(name)
          QString pretty_path;      // QString{"%1:%2"}.arg(chip).arg(line)
          pretty_path = QString{"%1/%2"}.arg(chip).arg(line);
          if(s_name.isEmpty() && s_label.isEmpty())
          {
            pretty_tree_name = s_line;
          }
          else if(s_name.isEmpty() && !s_label.isEmpty())
          {
            pretty_name = s_label;
            pretty_tree_name = QString{"%1: %2"}.arg(line).arg(s_label);
          }
          else if(!s_name.isEmpty() && s_label.isEmpty())
          {
            pretty_name = s_name;
            pretty_tree_name = QString{"%1: %2"}.arg(line).arg(s_name);
          }
          else
          {
            pretty_name = s_name;
            pretty_label = s_label;
            pretty_tree_name = QString{"%1: %2 (%3)"}.arg(line).arg(s_name).arg(s_label);
          }

          chip_node.emplace_back(
              SimpleIOData{
                  {.control = SimpleIO::
                       GPIO{.chip = chip, .line = line, .flags = 0, .events = 0, .direction = false},
                   .name = pretty_name,
                   .path = pretty_path},
                  pretty_tree_name,
                  pretty_label,
              },
              &chip_node);
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
          return node.tree_name.isEmpty() ? node.name : node.tree_name;
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
    setSelectionMode(QAbstractItemView::SingleSelection);
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
      , m_buttons{QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Cancel, this}
      , m_gpioIn{"In"}
      , m_gpioOut{"Out"}
      , m_gpioPullUp{"Pull-up"}
      , m_gpioPullDown{"Pull-down"}
      , m_pwmChannel{}
      , m_pwmPolarity{}
  {
    this->setLayout(&m_layout);
    m_layout.addWidget(&m_availablePorts);
    m_layout.addLayout(&m_setupLayoutContainer);
    m_layout.setStretch(0, 3);
    m_layout.setStretch(1, 5);

    m_availablePorts.setModel(&SimpleIODatabase::instance());
    m_availablePorts.header()->resizeSection(0, 180);
    m_availablePorts.onSelectionChanged = [&](const SimpleIONode& newFixt) {
      m_currentNode = nullptr;
      if(newFixt.childCount() > 0)
        return;

      updateParameters(newFixt);
    };
    m_pwmChannel.setRange(0, 32);

    m_setupLayoutContainer.addLayout(&m_setupLayout);
    m_setupLayout.addRow(tr("Name"), &m_name);
    m_setupLayout.addRow(tr("Info"), &m_info);
    m_setupLayout.addRow(tr("Conf"), &m_custom);
    m_setupLayoutContainer.addStretch(0);
    m_setupLayoutContainer.addWidget(&m_buttons);

    m_custom.addWidget(&m_defaultWidget);
    m_custom.addWidget(&m_gpioWidget);
    m_custom.addWidget(&m_pwmWidget);

    m_gpioWidget.setLayout(&m_gpioLayout);
    m_pwmWidget.setLayout(&m_pwmLayout);

    m_gpioLayout.addRow(tr("Chip"), &m_gpioChip);
    m_gpioLayout.addRow(tr("Line"), &m_gpioLine);
    m_gpioInOutLayout.addWidget(&m_gpioIn);
    m_gpioInOutLayout.addWidget(&m_gpioOut);
    m_gpioLayout.addRow(&m_gpioInOutLayout);

    m_gpioPullLayout.addWidget(&m_gpioPullUp);
    m_gpioPullLayout.addWidget(&m_gpioPullDown);
    m_gpioLayout.addRow(&m_gpioPullLayout);

    m_pwmLayout.addRow(tr("Chip"), &m_pwmChip);
    m_pwmLayout.addRow(tr("Channel"), &m_pwmChannel);
    m_pwmLayout.addRow(tr("Polarity"), &m_pwmPolarity);

    m_custom.setCurrentIndex(0);

    connect(&m_buttons, &QDialogButtonBox::accepted, this, &AddPortDialog::accept);
    connect(&m_buttons, &QDialogButtonBox::rejected, this, &AddPortDialog::reject);
  }

  void updateParameters(const SimpleIONode& fixt)
  {
    m_currentNode = &fixt;

    if(!fixt.name.isEmpty())
      m_name.setText(fixt.name);
    else if(!fixt.label.isEmpty())
      m_name.setText(fixt.label);
    else if(!fixt.path.isEmpty())
      m_name.setText(fixt.path);

    QString label;
    if(!fixt.label.isEmpty())
    {
      label += "<b>Label:</b> ";
      label += fixt.label;
    }

    if(!fixt.path.isEmpty())
    {
      if(!label.isEmpty())
        label += "<br/>";

      label += "<b>Path:</b> ";
      label += fixt.path;
    }

    m_info.setTextFormat(Qt::RichText);
    m_info.setText(label);

    switch(fixt.control.index())
    {
      case 0: // GPIO
        m_custom.setCurrentIndex(1);
        m_gpioIn.setChecked(true);
        m_gpioPullUp.setChecked(true);
        m_gpioChip.setValue(ossia::get<0>(fixt.control).chip);
        m_gpioLine.setValue(ossia::get<0>(fixt.control).line);
        break;
      case 1: // PWM
        m_custom.setCurrentIndex(2);
        m_pwmChannel.setValue(0);
        m_pwmChip.setValue(ossia::get<1>(fixt.control).chip);
        break;
      default:
        m_custom.setCurrentIndex(0);
        break;
    }
  }

  QSize sizeHint() const override { return QSize{800, 600}; }

  SimpleIO::Port port() const noexcept
  {
    SimpleIO::Port f;
    if(!m_currentNode)
      return f;
    f = *m_currentNode;
    f.name = m_name.text();

    struct setup
    {
      const AddPortDialog& self;
      void operator()(SimpleIO::GPIO& gpio) const noexcept
      {
        gpio.chip = self.m_gpioChip.value();
        gpio.line = self.m_gpioLine.value();
        if(self.m_gpioIn.isChecked())
        {
          gpio.direction = 0;
          gpio.flags |= GPIOHANDLE_REQUEST_INPUT;
        }
        else if(self.m_gpioOut.isChecked())
        {
          gpio.direction = 1;
          gpio.flags |= GPIOHANDLE_REQUEST_OUTPUT;
        }

        if(self.m_gpioPullUp.isChecked())
        {
          gpio.flags |= GPIOHANDLE_REQUEST_BIAS_PULL_UP;
        }
        if(self.m_gpioPullDown.isChecked())
        {
          gpio.flags |= GPIOHANDLE_REQUEST_BIAS_PULL_DOWN;
        }
      }

      void operator()(SimpleIO::PWM& pwm) const noexcept
      {
        pwm.chip = self.m_pwmChip.value();
        pwm.channel = self.m_pwmChannel.value();
        pwm.polarity = self.m_pwmPolarity.isChecked();
      }

      void operator()(SimpleIO::ADC& gpio) const noexcept { }
      void operator()(SimpleIO::DAC& gpio) const noexcept { }
      void operator()(SimpleIO::HID& gpio) const noexcept { }
      void operator()(SimpleIO::Custom& gpio) const noexcept { }
    };

    ossia::visit(setup{*this}, f.control);

    return f;
  }

private:
  QHBoxLayout m_layout;
  SimpleIOTreeView m_availablePorts;

  QVBoxLayout m_setupLayoutContainer;
  QFormLayout m_setupLayout;
  State::AddressFragmentLineEdit m_name;
  QLabel m_info;
  QDialogButtonBox m_buttons;

  QStackedLayout m_custom;

  QWidget m_defaultWidget;
  QWidget m_gpioWidget;
  QWidget m_pwmWidget;

  // GPIO
  QFormLayout m_gpioLayout;
  QHBoxLayout m_gpioInOutLayout;
  QHBoxLayout m_gpioPullLayout;
  QRadioButton m_gpioIn, m_gpioOut;
  QCheckBox m_gpioPullUp, m_gpioPullDown;
  QSpinBox m_gpioChip;
  QSpinBox m_gpioLine;

  // PWM
  QFormLayout m_pwmLayout;
  QSpinBox m_pwmChannel;
  QCheckBox m_pwmPolarity;
  QSpinBox m_pwmChip;

  // ADC
  QSpinBox m_adcExternalPort;

  // DAC
  QSpinBox m_dacExternalPort;

  const SimpleIOData* m_currentNode{};
};

SimpleIOProtocolSettingsWidget::SimpleIOProtocolSettingsWidget(QWidget* parent)
    : Device::ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  m_deviceNameEdit->setText("SimpleIO");

  m_boardLabel = new QLabel{"Local"};

  auto layout = new QFormLayout;
  layout->addRow(tr("Name"), m_deviceNameEdit);

  m_localPortsWidget = new QTableWidget;
  layout->addRow(m_localPortsWidget);
  
  m_localPortsWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_localPortsWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_localPortsWidget->insertColumn(0);
  m_localPortsWidget->insertColumn(1);
  m_localPortsWidget->insertColumn(2);
  m_localPortsWidget->setHorizontalHeaderLabels({tr("Name"), tr("Address"), tr("Type")});

  auto btns = new QHBoxLayout;
  m_localLoadLayout = new QPushButton{"Load a Wokwi layout"};
  m_localAddPort = new QPushButton{"Add a port"};
  m_localRmPort = new QPushButton{"Remove selected port"};
  btns->addWidget(m_localLoadLayout);
  btns->addWidget(m_localAddPort);
  btns->addWidget(m_localRmPort);
  {
    //

    auto& set = score::AppContext().settings<Library::Settings::Model>();
    QString pkg = set.getPackagesPath() + "/wokwi/wokwi-boards/boards";
    if(!QDir{pkg}.exists())
      layout->addRow(new QLabel{tr("Warning! Boards definitions not found.\nClone "
                                   "https://github.com/wokwi/wokwi-boards "
                                   "in\n%1/wokwi/")
                                    .arg(set.getPackagesPath())});
  }
  layout->addRow(btns);

  {
    QFrame* line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    layout->addRow(line);
  }
  layout->addRow(tr("Board"), m_boardLabel);

  m_transport = new QComboBox{this};
  m_transport->addItems(
      {"UDP", "TCP", "Serial port", "Unix Datagram", "Unix Stream", "Websocket Client",
       "Websocket Server"});
  layout->addRow(tr("OSC protocol"), m_transport);
  m_osc = new OSCTransportWidget{*this, this};
  m_osc->setEnabled(false);
  m_transport->setEnabled(false);
  m_osc->setConfiguration(ossia::net::osc_protocol_configuration{
      .transport = ossia::net::udp_configuration{
          {.local = ossia::net::inbound_socket_configuration{"0.0.0.0", 7765},
           .remote = ossia::net::outbound_socket_configuration{"192.168.1.77", 5567}}}});
  layout->addRow(m_osc);

  QObject::connect(
      m_transport, qOverload<int>(&QComboBox::currentIndexChanged), this,
      [this](int idx) { m_osc->setCurrentProtocol((OscProtocol)idx); });

  connect(m_localLoadLayout, &QPushButton::clicked, this, [this] {
    {
      QFileDialog d{qApp->activeWindow(), tr("Load Wokwi diagram.json")};
      d.setNameFilter("Wokwi diagram (*.json)");
      d.setFileMode(QFileDialog::ExistingFile);
      d.setAcceptMode(QFileDialog::AcceptOpen);
      if(d.exec())
      {
        auto files = d.selectedFiles();
        if(files.empty())
          return;
        if(QFile f{files[0]}; f.open(QIODevice::ReadOnly))
        {
          auto ba = readJson(score::mapAsByteArray(f));
          if(!ba.HasParseError())
          {
            m_impl = loadWokwi(ba);

            // No good: revert back to default state.
            if(m_impl.ports.empty())
              m_impl.board = "Local";

            updateGui();
          }
        }
      }
    }
  });
  connect(m_localAddPort, &QPushButton::clicked, this, [this] {
    auto dial = new AddPortDialog{*this};
    if(dial->exec() == QDialog::Accepted)
    {
      auto port = dial->port();
      if(!port.name.isEmpty() && !port.path.isEmpty())
      {
        m_impl.ports.push_back(port);
        updateTable();
      }
    }
  });
  connect(m_localRmPort, &QPushButton::clicked, this, [this] {
    ossia::flat_set<int> rows_to_remove;
    for(auto item : m_localPortsWidget->selectedItems())
    {
      rows_to_remove.insert(item->row());
    }

    for(auto it = rows_to_remove.rbegin(); it != rows_to_remove.rend(); ++it)
    {
      m_impl.ports.erase(m_impl.ports.begin() + *it);
    }

    updateTable();
  });

  setLayout(layout);
}

static QString typeName(const SimpleIO::Type& t)
{
  static constexpr struct
  {
    QString operator()(const SimpleIO::GPIO& gpio) const noexcept
    {
      return QStringLiteral("GPIO");
    }
    QString operator()(const SimpleIO::PWM& gpio) const noexcept
    {
      return QStringLiteral("PWM");
    }
    QString operator()(const SimpleIO::ADC& gpio) const noexcept
    {
      return QStringLiteral("ADC");
    }
    QString operator()(const SimpleIO::DAC& gpio) const noexcept
    {
      return QStringLiteral("DAC");
    }
    QString operator()(const SimpleIO::HID& gpio) const noexcept
    {
      return QStringLiteral("HID");
    }
    QString operator()(const SimpleIO::Custom& gpio) const noexcept
    {
      return QStringLiteral("Custom");
    }
  } vis;
  return ossia::visit(vis, t);
}

void SimpleIOProtocolSettingsWidget::updateGui()
{
  m_boardLabel->setText(m_impl.board);
  m_transport->setEnabled(m_impl.board != "Local");
  m_osc->setEnabled(m_impl.board != "Local");

  updateTable();
}

void SimpleIOProtocolSettingsWidget::updateTable()
{
  while(m_localPortsWidget->rowCount() > 0)
    m_localPortsWidget->removeRow(int(m_localPortsWidget->rowCount()) - 1);

  int row = 0;
  for(auto& fixt : m_impl.ports)
  {
    auto name_item = new QTableWidgetItem{fixt.name};
    auto address = new QTableWidgetItem{fixt.path};
    auto type_item = new QTableWidgetItem{typeName(fixt.control)};
    //auto controls = new QTableWidgetItem{QString::number(fixt.controls.size())};
    name_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    // mode_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    address->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    //controls->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    
    m_localPortsWidget->insertRow(row);
    m_localPortsWidget->setItem(row, 0, name_item);
    //m_portsWidget->setItem(row, 1, mode_item);
    m_localPortsWidget->setItem(row, 1, address);
    m_localPortsWidget->setItem(row, 2, type_item);
    row++;
  }
}
SimpleIOProtocolSettingsWidget::~SimpleIOProtocolSettingsWidget() { }

Device::DeviceSettings SimpleIOProtocolSettingsWidget::getSettings() const
{
  // TODO should be = m_settings to follow the other patterns.
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = SimpleIOProtocolFactory::static_concreteKey();
  auto stgs = m_impl;
  if(m_osc->isEnabled())
    stgs.osc_configuration
        = m_osc->configuration((OscProtocol)m_transport->currentIndex());
  else
    stgs.osc_configuration.reset();

  s.deviceSpecificSettings = QVariant::fromValue(stgs);

  return s;
}

void SimpleIOProtocolSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
  m_impl = settings.deviceSpecificSettings.value<SimpleIOSpecificSettings>();
  if(m_impl.osc_configuration)
  {
    auto proto = m_osc->setConfiguration(*m_impl.osc_configuration);
    m_transport->setCurrentIndex((int)proto);
    m_transport->setEnabled(true);
    m_osc->setEnabled(true);
  }
  else
  {
    m_transport->setCurrentIndex(0);
    m_transport->setEnabled(false);
    m_osc->setEnabled(false);
  }
  updateGui();
}
}
#endif
