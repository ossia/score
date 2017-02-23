#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <State/Widgets/AddressFragmentLineEdit.hpp>
#include <iscore/widgets/MarginLess.hpp>

#include <QPushButton>
#include <QSpinBox>
#include <QVariant>

#include "OSCProtocolSettingsWidget.hpp"
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include "OSCSpecificSettings.hpp"

class QWidget;

namespace Engine
{
namespace Network
{
OSCProtocolSettingsWidget::OSCProtocolSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};

  m_portInputSBox = new QSpinBox(this);
  m_portInputSBox->setRange(0, 65535);

  m_portOutputSBox = new QSpinBox(this);
  m_portOutputSBox->setRange(0, 65535);

  m_localHostEdit = new QLineEdit(this);

  QPushButton* loadNamespaceButton = new QPushButton(tr("Load..."), this);
  loadNamespaceButton->setAutoDefault(false);
  loadNamespaceButton->setToolTip(tr("Load namespace file"));
  m_namespaceFilePathEdit = new QLineEdit(this);

  auto layout = new QFormLayout;
  auto sublay = new iscore::MarginLess<QHBoxLayout>;
  sublay->addWidget(loadNamespaceButton);
  sublay->addWidget(m_namespaceFilePathEdit);
  auto load_label = new QLabel{tr("Namespace file"), this};
  load_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  layout->addRow(tr("Device name"), m_deviceNameEdit);
  layout->addRow(tr("Device listening port"), m_portInputSBox);
  layout->addRow(tr("i-score listening port"), m_portOutputSBox);
  layout->addRow(tr("Device host"), m_localHostEdit);
  layout->addRow(load_label, sublay);

  connect(
      loadNamespaceButton, &QAbstractButton::clicked, this,
      &OSCProtocolSettingsWidget::openFileDialog);

  setLayout(layout);

  setDefaults();
}

void OSCProtocolSettingsWidget::setDefaults()
{
  m_deviceNameEdit->setText("OSCdevice");
  m_portOutputSBox->setValue(9997);
  m_portInputSBox->setValue(9996);
  m_localHostEdit->setText("127.0.0.1");
}

Device::DeviceSettings OSCProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();

  Network::OSCSpecificSettings osc;
  osc.host = m_localHostEdit->text();
  osc.inputPort = m_portInputSBox->value();
  osc.outputPort = m_portOutputSBox->value();

  // TODO list.append(m_namespaceFilePathEdit->text());
  s.deviceSpecificSettings = QVariant::fromValue(osc);

  return s;
}

QString OSCProtocolSettingsWidget::getPath() const
{
  return m_namespaceFilePathEdit->text();
}

void OSCProtocolSettingsWidget::setSettings(
    const Device::DeviceSettings& settings)
{
  /*
      ISCORE_ASSERT(settings.size() == 5);

      m_namespaceFilePathEdit->setText(settings.at(4));
  */
  m_deviceNameEdit->setText(settings.name);
  Network::OSCSpecificSettings osc;
  if (settings.deviceSpecificSettings
          .canConvert<Network::OSCSpecificSettings>())
  {
    osc = settings.deviceSpecificSettings
              .value<Network::OSCSpecificSettings>();
    m_portInputSBox->setValue(osc.inputPort);
    m_portOutputSBox->setValue(osc.outputPort);
    m_localHostEdit->setText(osc.host);
  }
}

void OSCProtocolSettingsWidget::openFileDialog()
{
  const QString fileName = QFileDialog::getOpenFileName(
      this, tr("Open Device file"), QString{},
      tr("Device file (*.xml *.device *.json)"));

  if (!fileName.isEmpty())
  {
    m_namespaceFilePathEdit->setText(fileName);
  }
}
}
}
