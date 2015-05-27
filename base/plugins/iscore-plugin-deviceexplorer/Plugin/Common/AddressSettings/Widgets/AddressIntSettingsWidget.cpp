#include "AddressIntSettingsWidget.hpp"

#include <QComboBox>
#include <QDebug>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>

#include <Common/CommonTypes.hpp>
#include "Common/AddressSettings/AddressSettings.hpp"
#include "Common/AddressSettings/AddressSpecificSettings/AddressIntSettings.hpp"

AddressIntSettingsWidget::AddressIntSettingsWidget(QWidget* parent)
    : AddressSettingsWidget(parent)
{
    buildGUI();
}

void
AddressIntSettingsWidget::buildGUI()
{
    QLabel* ioTypeLabel = new QLabel(tr("I/O type"), this);
    m_ioTypeCBox = new QComboBox(this);

    QLabel* valueLabel = new QLabel(tr("Value"), this);
    m_valueSBox = new QSpinBox(this);

    QLabel* minLabel = new QLabel(tr("Min"), this);
    m_minSBox = new QSpinBox(this);

    QLabel* maxLabel = new QLabel(tr("Max"), this);
    m_maxSBox = new QSpinBox(this);

    QLabel* unitLabel = new QLabel(tr("Unit"), this);
    m_unitCBox = new QComboBox(this);

    QLabel* clipModeLabel  = new QLabel(tr("Clip mode"), this);
    m_clipModeCBox = new QComboBox(this);

    QLabel* priorityLabel  = new QLabel(tr("Priority"), this);
    m_prioritySBox = new QSpinBox(this);

    QLabel* tagsLabel  = new QLabel(tr("Tags"), this);
    m_tagsEdit = new QLineEdit(this);

    QGridLayout* gLayout = new QGridLayout;

    gLayout->addWidget(ioTypeLabel, 0, 0, 1, 1);
    gLayout->addWidget(m_ioTypeCBox, 0, 1, 1, 1);

    gLayout->addWidget(valueLabel, 1, 0, 1, 1);
    gLayout->addWidget(m_valueSBox, 1, 1, 1, 1);

    gLayout->addWidget(unitLabel, 1, 2, 1, 1);
    gLayout->addWidget(m_unitCBox, 1, 3, 1, 1);

    gLayout->addWidget(minLabel, 2, 0, 1, 1);
    gLayout->addWidget(m_minSBox, 2, 1, 1, 1);

    gLayout->addWidget(maxLabel, 2, 2, 1, 1);
    gLayout->addWidget(m_maxSBox, 2, 3, 1, 1);

    gLayout->addWidget(clipModeLabel, 3, 0, 1, 1);
    gLayout->addWidget(m_clipModeCBox, 3, 1, 1, 1);

    gLayout->addWidget(priorityLabel, 4, 0, 1, 1);
    gLayout->addWidget(m_prioritySBox, 4, 1, 1, 1);

    gLayout->addWidget(tagsLabel, 5, 0, 1, 1);
    gLayout->addWidget(m_tagsEdit, 5, 1, 1, 1);

    populateIOTypes(m_ioTypeCBox);
    populateUnit(m_unitCBox);
    populateClipMode(m_clipModeCBox);

    setLayout(gLayout);

    setDefaults();
}

void
AddressIntSettingsWidget::setDefaults()
{
    Q_ASSERT(m_ioTypeCBox);
    Q_ASSERT(m_valueSBox);

    m_ioTypeCBox->setCurrentIndex(0);

    m_valueSBox->setValue(0);
    m_valueSBox->setMinimum(-std::numeric_limits<int>::max());
    m_valueSBox->setMaximum(std::numeric_limits<int>::max());

    m_minSBox->setMinimum(-std::numeric_limits<int>::max());
    m_minSBox->setMaximum(std::numeric_limits<int>::max());
    m_minSBox->setValue(0);

    m_maxSBox->setMinimum(-std::numeric_limits<int>::max());
    m_maxSBox->setMaximum(std::numeric_limits<int>::max());
    m_maxSBox->setValue(100);

    m_unitCBox->setCurrentIndex(0);

    m_clipModeCBox->setCurrentIndex(0);

    m_prioritySBox->setMinimum(0);
    m_prioritySBox->setMaximum(std::numeric_limits<int>::max());
    m_prioritySBox->setSingleStep(1);
    m_prioritySBox->setValue(0);

    m_tagsEdit->setText("");
}

AddressSettings AddressIntSettingsWidget::getSettings() const
{
    Q_ASSERT(m_ioTypeCBox);

    AddressSettings settings;
    settings.ioType = IOTypeStringMap().key(m_ioTypeCBox->currentText());
    settings.priority = m_prioritySBox->value();
    settings.tags = m_tagsEdit->text();
    settings.value = m_valueSBox->value();
    settings.valueType = QString("Int");

    AddressIntSettings is;
    is.clipMode = m_clipModeCBox->currentText();
    is.max = m_maxSBox->value();
    is.min = m_minSBox->value();
    is.unit = m_unitCBox->currentText();

    settings.addressSpecificSettings = QVariant::fromValue(is);

    return settings;
}

void
AddressIntSettingsWidget::setSettings(const AddressSettings &settings)
{
    Q_ASSERT(m_ioTypeCBox);

    const int ioTypeIndex = m_ioTypeCBox->findText(IOTypeStringMap()[settings.ioType]);
    Q_ASSERT(ioTypeIndex != -1);

    m_ioTypeCBox->setCurrentIndex(ioTypeIndex);

    if(settings.addressSpecificSettings.canConvert<AddressIntSettings>())
    {
        AddressIntSettings iSettings = settings.addressSpecificSettings.value<AddressIntSettings>();

        if (settings.value.canConvert<float>())
        {
            int val = settings.value.value<float>();
            m_valueSBox->setValue(val);
        }

        m_minSBox->setValue(iSettings.min);
        m_maxSBox->setValue(iSettings.max);

        const QString& unitString = iSettings.unit;
        const int unitIndex = m_unitCBox->findText(unitString);

        if(unitIndex != -1)
        {
            m_unitCBox->setCurrentIndex(unitIndex);
        }
        else
        {
            qDebug() << tr("Unknown unit type: %1").arg(unitString) << "\n";
        }

        const QString& clipModeString = iSettings.clipMode ;
        const int clipModeIndex = m_clipModeCBox->findText(clipModeString);

        if(clipModeIndex != -1)
        {
            m_clipModeCBox->setCurrentIndex(clipModeIndex);
        }
        else
        {
            qDebug() << tr("Unknown clip mode: %1").arg(clipModeString) << "\n";
        }
    }
    else
    {
        qDebug() << tr("Error when loading address specific settings") << "\n";
    }

    m_prioritySBox->setValue(settings.priority);

    m_tagsEdit->setText(settings.tags );
}


