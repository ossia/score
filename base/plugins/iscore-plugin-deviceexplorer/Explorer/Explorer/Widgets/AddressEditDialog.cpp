#include <Explorer/Common/AddressSettings/AddressSettingsFactory.hpp>
#include <Explorer/Common/AddressSettings/Widgets/AddressSettingsWidget.hpp>
#include <Explorer/Widgets/AddressFragmentLineEdit.hpp>
#include <State/ValueConversion.hpp>
#include <QComboBox>
#include <QDebug>
#include <QDialogButtonBox>
#include <QFlags>
#include <QFormLayout>
#include <QLineEdit>
#include <qnamespace.h>

#include <QString>

#include "AddressEditDialog.hpp"
#include <Device/Address/AddressSettings.hpp>
#include <Device/Address/Domain.hpp>
#include <Device/Address/IOType.hpp>
#include <Explorer/Widgets/ValueWrapper.hpp>
#include <State/Value.hpp>

class QWidget;

namespace DeviceExplorer
{
AddressEditDialog::AddressEditDialog(
        QWidget* parent):
    AddressEditDialog{makeDefaultSettings(), parent}
{
}

AddressEditDialog::AddressEditDialog(
        const Device::AddressSettings& addr,
        QWidget* parent)
    : QDialog{parent},
      m_originalSettings{addr}
{
    m_layout = new QFormLayout;
    setLayout(m_layout);

    // Name
    m_nameEdit = new AddressFragmentLineEdit;
    m_layout->addRow(tr("Name"), m_nameEdit);

    setNodeSettings();

    // Value type
    m_valueTypeCBox = new QComboBox(this);
    m_valueTypeCBox->addItems(AddressSettingsFactory::instance().getAvailableValueTypes());

    connect(m_valueTypeCBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &AddressEditDialog::updateType);

    m_layout->addRow(tr("Value type"), m_valueTypeCBox);

    // AddressWidget
    m_addressWidget = new WidgetWrapper<AddressSettingsWidget>{this};
    m_layout->addWidget(m_addressWidget);

    setValueSettings();

    // Ok / Cancel
    auto buttonBox = new QDialogButtonBox{QDialogButtonBox::Ok
            | QDialogButtonBox::Cancel, Qt::Horizontal, this};
    connect(buttonBox, &QDialogButtonBox::accepted,
            this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);

    m_layout->addWidget(buttonBox);
}

AddressEditDialog::~AddressEditDialog()
{

}

void AddressEditDialog::updateType()
{
    const QString valueType = m_valueTypeCBox->currentText();
    m_addressWidget->setWidget(AddressSettingsFactory::instance().getValueTypeWidget(valueType));
    if(m_originalSettings.ioType == Device::IOType::Invalid)
        m_originalSettings.ioType = Device::IOType::InOut;
    m_addressWidget->widget()->setSettings(m_originalSettings);
}


Device::AddressSettings AddressEditDialog::getSettings() const
{
    Device::AddressSettings settings;

    if(m_addressWidget && m_addressWidget->widget())
    {
        settings = m_addressWidget->widget()->getSettings();
    }
    else
    {
        // Int by default
        settings.value.val = 0;
    }

    settings.name = m_nameEdit->text();

    return settings;
}

Device::AddressSettings AddressEditDialog::makeDefaultSettings()
{
    static Device::AddressSettings defaultSettings
            = [] () {
        Device::AddressSettings s;
        s.value = State::Value::fromValue(0);
        s.domain.min = State::Value::fromValue(0);
        s.domain.max = State::Value::fromValue(100);
        s.ioType = Device::IOType::InOut;
        return s;
    }();

    return defaultSettings;
}

void AddressEditDialog::setNodeSettings()
{
    const QString name = m_originalSettings.name;
    m_nameEdit->setText(name);
}

void AddressEditDialog::setValueSettings()
{
    const int index = m_valueTypeCBox->findText(State::convert::prettyType(m_originalSettings.value));
    ISCORE_ASSERT(index != -1);
    ISCORE_ASSERT(index < m_valueTypeCBox->count());
    if(m_valueTypeCBox->currentIndex() == index)
    {
        m_valueTypeCBox->currentIndexChanged(index);
    }
    else
    {
        m_valueTypeCBox->setCurrentIndex(index);  //will emit currentIndexChanged(int) & call slot
    }

}
}
