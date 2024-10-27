#include "libsimpleio/libgpio.h"
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_SIMPLEIO)
#include "SimpleIOProtocolFactory.hpp"
#include "SimpleIOProtocolSettingsWidget.hpp"
#include "SimpleIOSpecificSettings.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/model/tree/TreeNodeItemModel.hpp>

#include <ossia/detail/flat_map.hpp>
#include <ossia/detail/hash_map.hpp>
#include <ossia/detail/math.hpp>
#include <ossia/detail/string_algorithms.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDirIterator>
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

    m_gpioInOutLayout.addWidget(&m_gpioIn);
    m_gpioInOutLayout.addWidget(&m_gpioOut);
    m_gpioLayout.addRow(&m_gpioInOutLayout);

    m_gpioPullLayout.addWidget(&m_gpioPullUp);
    m_gpioPullLayout.addWidget(&m_gpioPullDown);
    m_gpioLayout.addRow(&m_gpioPullLayout);

    m_pwmLayout.addRow("Channel", &m_pwmChannel);
    m_pwmLayout.addRow("Polarity", &m_pwmPolarity);

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
        break;
      case 1: // PWM
        m_custom.setCurrentIndex(2);
        m_pwmChannel.setValue(0);
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
  QFormLayout m_gpioLayout;
  QHBoxLayout m_gpioInOutLayout;
  QHBoxLayout m_gpioPullLayout;
  QRadioButton m_gpioIn, m_gpioOut;
  QCheckBox m_gpioPullUp, m_gpioPullDown;

  QFormLayout m_pwmLayout;
  QSpinBox m_pwmChannel;
  QCheckBox m_pwmPolarity;

  const SimpleIOData* m_currentNode{};
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
  m_portsWidget->setHorizontalHeaderLabels({tr("Name"), tr("Address"), tr("Type")});

  auto btns = new QHBoxLayout;
  m_addPort = new QPushButton{"Add a port"};
  m_rmPort = new QPushButton{"Remove selected port"};
  btns->addWidget(m_addPort);
  btns->addWidget(m_rmPort);
  layout->addRow(btns);

  connect(m_addPort, &QPushButton::clicked, this, [this] {
    auto dial = new AddPortDialog{*this};
    if(dial->exec() == QDialog::Accepted)
    {
      auto port = dial->port();
      if(!port.name.isEmpty() && !port.path.isEmpty())
      {
        m_ports.push_back(port);
        updateTable();
      }
    }
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
void SimpleIOProtocolSettingsWidget::updateTable()
{
  while(m_portsWidget->rowCount() > 0)
    m_portsWidget->removeRow(int(m_portsWidget->rowCount()) - 1);

  int row = 0;
  for(auto& fixt : m_ports)
  {
    auto name_item = new QTableWidgetItem{fixt.name};
    auto address = new QTableWidgetItem{fixt.path};
    auto type_item = new QTableWidgetItem{typeName(fixt.control)};
    //auto controls = new QTableWidgetItem{QString::number(fixt.controls.size())};
    name_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    // mode_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    address->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    //controls->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

    m_portsWidget->insertRow(row);
    m_portsWidget->setItem(row, 0, name_item);
    //m_portsWidget->setItem(row, 1, mode_item);
    m_portsWidget->setItem(row, 1, address);
    m_portsWidget->setItem(row, 2, type_item);
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
