#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFlags>
#include <QGridLayout>
#include <QLabel>
#include <QLayoutItem>
#include <qnamespace.h>

#include <QFormLayout>
#include <QVariant>
#include <QWidget>
#include <utility>

#include "DeviceEditDialog.hpp"
#include <Device/Protocol/ProtocolList.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/widgets/SignalUtils.hpp>
#include <iscore/widgets/TextLabel.hpp>

namespace Explorer
{
DeviceEditDialog::DeviceEditDialog(
    const Device::ProtocolFactoryList& pl, QWidget* parent)
    : QDialog(parent)
    , m_protocolList{pl}
    , m_protocolWidget{nullptr}
    , m_index(-1)
{
  setModal(true);
  buildGUI();
  setMinimumWidth(400);
}

DeviceEditDialog::~DeviceEditDialog()
{
}

void DeviceEditDialog::buildGUI()
{
  QDialogButtonBox* buttonBox = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
  connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

  m_layout = new QFormLayout;
  setLayout(m_layout);
  m_layout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

  // QLabel for the warning text
  m_layout->addWidget(new TextLabel);

  m_protocolCBox = new QComboBox(this);
  m_layout->addRow(tr("Protocol"), m_protocolCBox);
  m_layout->addWidget(buttonBox);

  initAvailableProtocols(); // populate m_protocolCBox

  connect(
      m_protocolCBox, SignalUtils::QComboBox_currentIndexChanged_int(), this,
      &DeviceEditDialog::updateProtocolWidget);

  if (m_protocolCBox->count() > 0)
  {
    ISCORE_ASSERT(m_protocolCBox->currentIndex() == 0);
    updateProtocolWidget();
  }
}

void DeviceEditDialog::initAvailableProtocols()
{
  ISCORE_ASSERT(m_protocolCBox);

  // initialize previous settings
  m_previousSettings.clear();

  std::vector<Device::ProtocolFactory*> sorted;
  for (auto& elt : m_protocolList)
  {
    sorted.push_back(&elt);
  }

  ossia::sort(
      sorted, [](Device::ProtocolFactory* lhs, Device::ProtocolFactory* rhs) {
        return lhs->visualPriority() > rhs->visualPriority();
      });

  for (const auto& prot_pair : sorted)
  {
    auto& prot = *prot_pair;
    m_protocolCBox->addItem(
        prot.prettyName(), QVariant::fromValue(prot.concreteKey()));

    m_previousSettings.append(prot.defaultSettings());
  }

  m_protocolCBox->setCurrentIndex(0);
  m_index = m_protocolCBox->currentIndex();
}

void DeviceEditDialog::updateProtocolWidget()
{
  ISCORE_ASSERT(m_protocolCBox);

  if (m_protocolWidget)
  {
    ISCORE_ASSERT(m_index < m_protocolCBox->count());
    ISCORE_ASSERT(m_index < m_previousSettings.count());

    m_previousSettings[m_index] = m_protocolWidget->getSettings();
    delete m_protocolWidget;
  }

  m_index = m_protocolCBox->currentIndex();

  auto protocol = m_protocolCBox->currentData()
                      .value<UuidKey<Device::ProtocolFactory>>();
  m_protocolWidget = m_protocolList.get(protocol)->makeSettingsWidget();

  if (m_protocolWidget)
  {
    m_layout->insertRow(2, m_protocolWidget);
    QSizePolicy pol{QSizePolicy::MinimumExpanding,
                    QSizePolicy::MinimumExpanding};
    pol.setVerticalStretch(1);
    m_protocolWidget->setSizePolicy(pol);
    m_protocolWidget->setMinimumHeight(200);
    this->setSizePolicy(
        QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    updateGeometry();
  }
}

Device::DeviceSettings DeviceEditDialog::getSettings() const
{
  Device::DeviceSettings settings;

  if (m_protocolWidget)
  {
    settings = m_protocolWidget->getSettings();
  }

  // TODO after set the protocol in getSettings instead.
  settings.protocol = m_protocolCBox->currentData()
                          .value<UuidKey<Device::ProtocolFactory>>();

  return settings;
}

QString DeviceEditDialog::getPath() const
{
  // TODO do this properly please
  return m_protocolWidget->getPath();
}

void DeviceEditDialog::setSettings(const Device::DeviceSettings& settings)
{
  // auto proto = SingletonProtocolList::instance().get(settings.protocol);
  // if(proto)
  {
    const int index
        = m_protocolCBox->findData(QVariant::fromValue(settings.protocol));
    ISCORE_ASSERT(index != -1);
    ISCORE_ASSERT(index < m_protocolCBox->count());

    m_protocolCBox->setCurrentIndex(
        index); // will emit currentIndexChanged(int) & call slot

    m_protocolWidget->setSettings(settings);
  }
  // else
  {
    //    ISCORE_TODO; // Make a default widget.
  }
}

void DeviceEditDialog::setEditingInvalidState(bool st)
{
  if (st != m_invalidState)
  {
    if (st)
    {
      auto item = m_layout->itemAt(0);
      static_cast<QLabel*>(item->widget())
          ->setText(tr("Warning : device requires editing."));
    }
    else
    {
      auto item = m_layout->takeAt(0);
      delete item->widget();
      delete item;
    }

    m_invalidState = st;
  }
}
}
