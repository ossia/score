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

AddressEditDialog::AddressEditDialog(
        QWidget* parent):
    AddressEditDialog{makeDefaultSettings(), parent}
{
}

AddressEditDialog::AddressEditDialog(
        const iscore::AddressSettings& addr,
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


    if(m_originalSettings.ioType != iscore::IOType::Invalid)
    {
        // Value type
        m_valueTypeCBox = new QComboBox(this);
        m_valueTypeCBox->addItems(AddressSettingsFactory::instance().getAvailableValueTypes());
        //m_valueTypeCBox->setEnabled(false); // Note : the day where the OSSIA API will be able to change the type of an address.
        connect(m_valueTypeCBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                this, &AddressEditDialog::updateType);

        m_layout->addRow(tr("Value type"), m_valueTypeCBox);

        // AddressWidget
        m_addressWidget = new WidgetWrapper<AddressSettingsWidget>{this};
        m_layout->addWidget(m_addressWidget);

        setValueSettings();
    }
    else
    {
        // TODO Make a way to add addresses when there are none.
        // e. g. "Add address" and "Remove address" button.
        ISCORE_TODO;
    }

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
    m_addressWidget->widget()->setSettings(m_originalSettings);
}


iscore::AddressSettings AddressEditDialog::getSettings() const
{
    iscore::AddressSettings settings;

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

iscore::AddressSettings AddressEditDialog::makeDefaultSettings()
{
    static iscore::AddressSettings defaultSettings
            = [] () {
        iscore::AddressSettings s;
        s.value = iscore::Value::fromValue(0);
        s.domain.min = iscore::Value::fromValue(0);
        s.domain.max = iscore::Value::fromValue(100);
        s.ioType = iscore::IOType::InOut;
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
    const int index = m_valueTypeCBox->findText(iscore::convert::prettyType(m_originalSettings.value));
    ISCORE_ASSERT(index != -1);
    ISCORE_ASSERT(index < m_valueTypeCBox->count());
    m_valueTypeCBox->setCurrentIndex(index);  //will emit currentIndexChanged(int) & call slot
}
