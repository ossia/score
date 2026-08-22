#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_CAN)
#include "CANInterfaces.hpp"
#include "CANProtocolFactory.hpp"
#include "CANProtocolSettingsWidget.hpp"
#include "CANSpecificSettings.hpp"
#include "DBCParser.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <score/widgets/HelpInteraction.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Protocols::CANProtocolSettingsWidget)

namespace Protocols
{
CANProtocolSettingsWidget::CANProtocolSettingsWidget(QWidget* parent)
    : Device::ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  m_deviceNameEdit->setText("CAN");
  checkForChanges(m_deviceNameEdit);

  // Editable: the interface may legitimately not exist yet when the score is
  // written (the adapter is plugged in later, or the file comes from another
  // machine), so the user must be able to type a name that is not in the list.
  m_interface = new QComboBox{this};
  m_interface->setEditable(true);
  m_interface->addItems(CAN::availableInterfaceNames());
  checkForChanges(m_interface);

  // No fallback to "can0" here: a name that resolves to nothing looks like a
  // valid setting and only fails at connection time. An empty field plus the
  // warning below says what is actually going on.
  m_noInterface = new QLabel{
      tr("No CAN interface found on this machine.\n"
         "Plug an adapter in and bring it up, e.g.:\n"
         "  sudo ip link set can0 up type can bitrate 1000000\n"
         "or create a virtual bus for testing:\n"
         "  sudo modprobe vcan\n"
         "  sudo ip link add dev vcan0 type vcan && sudo ip link set up vcan0"),
      this};
  m_noInterface->setWordWrap(true);
  m_noInterface->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_noInterface->setVisible(m_interface->count() == 0);

  m_dbcPath = new QLineEdit{this};
  m_dbcPath->setPlaceholderText(tr("Path to a .dbc database"));
  checkForChanges(m_dbcPath);

  auto browse = new QPushButton{tr("Browse..."), this};
  connect(browse, &QPushButton::clicked, this, &CANProtocolSettingsWidget::browseDBC);

  auto dbcLayout = new QHBoxLayout{};
  dbcLayout->addWidget(m_dbcPath);
  dbcLayout->addWidget(browse);

  m_nodeIdOffset = new QSpinBox{this};
  // Wide enough to move a database anywhere inside the 29-bit space, in either
  // direction; the parser refuses individual moves that would leave the range.
  m_nodeIdOffset->setRange(-0x1FFFFFFF, 0x1FFFFFFF);
  m_nodeIdOffset->setValue(0);
  checkForChanges(m_nodeIdOffset);

  m_float32Override = new QCheckBox{this};
  m_float32Override->setChecked(false);
  checkForChanges(m_float32Override);

  m_fd = new QCheckBox{this};
  checkForChanges(m_fd);

  m_filterToDatabase = new QCheckBox{this};
  m_filterToDatabase->setChecked(true);
  checkForChanges(m_filterToDatabase);

  m_summary = new QLabel{this};
  m_summary->setWordWrap(true);
  m_summary->setTextInteractionFlags(Qt::TextSelectableByMouse);

  connect(
      m_dbcPath, &QLineEdit::textChanged, this,
      &CANProtocolSettingsWidget::updateDBCSummary);
  connect(
      m_nodeIdOffset, qOverload<int>(&QSpinBox::valueChanged), this,
      &CANProtocolSettingsWidget::updateDBCSummary);

  auto layout = new QFormLayout;
  layout->addRow(tr("Name"), m_deviceNameEdit);
  layout->addRow(tr("Interface"), m_interface);
  layout->addRow(QString{}, m_noInterface);
  score::setHelp(
      m_interface, tr("The SocketCAN network interface, e.g. can0 or vcan0.\n"
                      "Bring it up first, e.g.:\n"
                      "  sudo ip link set can0 up type can bitrate 1000000"));

  layout->addRow(tr("DBC file"), dbcLayout);
  layout->addRow(tr("Node id offset"), m_nodeIdOffset);
  score::setHelp(
      m_nodeIdOffset,
      tr("Added to every message id of the database.\n"
         "A DBC usually describes one device at one node id: a sensor whose file "
         "is written for CANopen node 1 (0x181, 0x281...) is reached at node 2 "
         "with an offset of 1.\n"
         "This lets one file serve a whole chain of devices on one bus, one "
         "score device per sensor."));

  layout->addRow(tr("32-bit ints are floats"), m_float32Override);
  score::setHelp(
      m_float32Override,
      tr("Decode every 32-bit integer signal as an IEEE 754 float instead.\n\n"
         "Leave this off unless the database is known to be wrong. It exists "
         "because some vendor files declare a float payload as a scaled integer "
         "and omit the SIG_VALTYPE_ record that would say otherwise; decoding "
         "such a file as written yields garbage.\n"
         "Signals with an explicit float or double type are never affected."));

  layout->addRow(tr("CAN FD"), m_fd);
  score::setHelp(
      m_fd, tr("Allow payloads larger than 8 bytes. Classic frames keep working."));

  layout->addRow(tr("Filter to database"), m_filterToDatabase);
  score::setHelp(
      m_filterToDatabase,
      tr("Ask the kernel to drop frames whose id is not in the database.\n"
         "Filters are per socket, so several devices may share one bus without "
         "paying for each other's traffic."));

  layout->addRow(QString{}, m_summary);

  setLayout(layout);

  updateDBCSummary();
}

CANProtocolSettingsWidget::~CANProtocolSettingsWidget() { }

void CANProtocolSettingsWidget::browseDBC()
{
  const auto path = QFileDialog::getOpenFileName(
      this, tr("Open a CAN database"), QFileInfo{m_dbcPath->text()}.absolutePath(),
      tr("CAN databases (*.dbc);;All files (*)"));

  if(!path.isEmpty())
    m_dbcPath->setText(path);
}

void CANProtocolSettingsWidget::updateDBCSummary()
{
  const auto path = m_dbcPath->text();
  if(path.isEmpty())
  {
    m_summary->clear();
    return;
  }

  auto db = CAN::parseDBCFile(path.toStdString());
  CAN::applyNodeIdOffset(db, m_nodeIdOffset->value());

  if(db.messages.empty())
  {
    m_summary->setText(tr("No message could be read from this file."));
    return;
  }

  std::size_t signals_n = 0;
  for(const auto& m : db.messages)
    signals_n += m.signals.size();

  // Show the identifiers with the offset already applied: the point of the
  // offset is that the user can check it against the device in front of them.
  QStringList ids;
  for(const auto& m : db.messages)
    ids.push_back(QString{"0x%1"}.arg(m.id, 0, 16));

  QString text = tr("%1 messages, %2 signals: %3")
                     .arg(db.messages.size())
                     .arg(signals_n)
                     .arg(ids.join(", "));

  if(!db.warnings.empty())
    text += "\n"
            + tr("%1 warning(s), first: %2")
                  .arg(db.warnings.size())
                  .arg(QString::fromStdString(db.warnings.front()));

  m_summary->setText(text);
}

Device::DeviceSettings CANProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = CANProtocolFactory::static_concreteKey();

  CANSpecificSettings settings{};
  settings.interfaceName = m_interface->currentText();
  settings.dbcPath = m_dbcPath->text();
  settings.nodeIdOffset = m_nodeIdOffset->value();
  settings.float32Override = m_float32Override->isChecked();
  settings.fd = m_fd->isChecked();
  settings.filterToDatabase = m_filterToDatabase->isChecked();
  s.deviceSpecificSettings = QVariant::fromValue(settings);

  return s;
}

void CANProtocolSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);

  const auto& specif = settings.deviceSpecificSettings.value<CANSpecificSettings>();

  if(!specif.interfaceName.isEmpty())
  {
    // The combo is editable precisely so a score written elsewhere -- or before
    // the adapter was plugged in -- keeps the name it was saved with instead of
    // being silently rewritten to whatever this machine happens to have.
    m_interface->setCurrentText(specif.interfaceName);
    m_noInterface->setVisible(false);
  }

  m_dbcPath->setText(specif.dbcPath);
  m_nodeIdOffset->setValue(specif.nodeIdOffset);
  m_float32Override->setChecked(specif.float32Override);
  m_fd->setChecked(specif.fd);
  m_filterToDatabase->setChecked(specif.filterToDatabase);

  updateDBCSummary();
}
}
#endif
