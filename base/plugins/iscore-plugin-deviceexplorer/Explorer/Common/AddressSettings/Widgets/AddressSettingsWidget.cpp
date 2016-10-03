#include <QBoxLayout>
#include <QComboBox>
#include <QFormLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QMap>
#include <QCheckBox>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QLabel>
#include "AddressSettingsWidget.hpp"
#include <Device/Address/AddressSettings.hpp>
#include <Device/Address/ClipMode.hpp>
#include <Device/Address/IOType.hpp>

#include <ossia/editor/dataspace/dataspace_visitors.hpp>

namespace Explorer
{
AddressSettingsWidget::AddressSettingsWidget(QWidget *parent) :
    QWidget(parent),
    m_layout{new QFormLayout},
    m_none_type{false}
{
    m_ioTypeCBox = new QComboBox{this};
    m_clipModeCBox = new QComboBox{this};
    m_repetition = new QCheckBox;
    m_tagsEdit = new QComboBox{this};
    m_tagsEdit->setEditable(true);
    m_tagsEdit->setInsertPolicy(QComboBox::InsertAtCurrent);
    auto addTagButton = new QPushButton;
    addTagButton->setText("+");
    connect(addTagButton, &QPushButton::clicked,
            this, [&] () {
        bool ok = false;
        auto res = QInputDialog::getText(
                    this,
                    tr("Add tag"),
                    tr("Add a tag"),
                    QLineEdit::Normal,
                    QString{}, &ok);
        if(ok)
        {
            m_tagsEdit->addItem(res);
        }
    });

    QHBoxLayout* tagLayout = new QHBoxLayout;
    tagLayout->addWidget(m_tagsEdit);
    tagLayout->addWidget(addTagButton);

    m_unit = new QLabel;
    m_description = new QLabel;

    m_layout->addRow(tr("I/O type"), m_ioTypeCBox);
    m_layout->addRow(tr("Clip mode"), m_clipModeCBox);
    m_layout->addRow(tr("Repetition filter"), m_repetition);
    m_layout->addRow(tr("Tags"), tagLayout);
    m_layout->addRow(tr("Unit"), m_unit);
    m_layout->addRow(tr("Description"), m_description);

    // Populate the combo boxes

    const auto& io_map = Device::IOTypeStringMap();
    for(auto it = io_map.cbegin(); it != io_map.cend(); ++it)
    {
        m_ioTypeCBox->addItem(it.value(), (int)it.key());
    }

    const auto& clip_map = Device::ClipModePrettyStringMap();
    for(auto it = clip_map.cbegin(); it != clip_map.cend(); ++it)
    {
        m_clipModeCBox->addItem(it.value(), (int)it.key());
    }

    setLayout(m_layout);
}

AddressSettingsWidget::AddressSettingsWidget(
        AddressSettingsWidget::no_widgets_t,
        QWidget* parent):
    QWidget(parent),
    m_layout{new QFormLayout},
    m_none_type{true}
{
    m_tagsEdit = new QComboBox{this};
    m_tagsEdit->setEditable(true);
    m_tagsEdit->setInsertPolicy(QComboBox::InsertAtCurrent);
    auto addTagButton = new QPushButton;
    addTagButton->setText("+");
    connect(addTagButton, &QPushButton::clicked,
            this, [&] () {
        bool ok = false;
        auto res = QInputDialog::getText(this, tr("Add tag"), tr("Add a tag"), QLineEdit::Normal, QString{}, &ok);
        if(ok)
        {
            m_tagsEdit->addItem(res);
        }
    });

    QHBoxLayout* tagLayout = new QHBoxLayout;
    tagLayout->addWidget(m_tagsEdit);
    tagLayout->addWidget(addTagButton);

    m_layout->addRow(tr("Tags"), tagLayout);

    setLayout(m_layout);
}

AddressSettingsWidget::~AddressSettingsWidget() = default;

Device::AddressSettings AddressSettingsWidget::getCommonSettings() const
{
    Device::AddressSettings settings;
    if(!m_none_type)
    {
        settings.ioType = static_cast<Device::IOType>(m_ioTypeCBox->currentData().value<int>());
        settings.clipMode = static_cast<Device::ClipMode>(m_clipModeCBox->currentData().value<int>());
        settings.repetitionFilter = m_repetition->isChecked();
    }

    for(int i = 0; i < m_tagsEdit->count(); i++)
        settings.tags.append(m_tagsEdit->itemText(i));

    return settings;
}

void AddressSettingsWidget::setCommonSettings(const Device::AddressSettings & settings)
{
    if(!m_none_type)
    {
        const int ioTypeIndex = m_ioTypeCBox->findData((int)settings.ioType);
        ISCORE_ASSERT(ioTypeIndex != -1);
        m_ioTypeCBox->setCurrentIndex(ioTypeIndex);

        const int clipModeIndex = m_clipModeCBox->findData((int)settings.clipMode);
        ISCORE_ASSERT(clipModeIndex != -1);
        m_clipModeCBox->setCurrentIndex(clipModeIndex);

        m_repetition->setChecked(settings.repetitionFilter);

        m_unit->setText(
                    QString::fromStdString(
                        ossia::get_dataspace_text(settings.unit).to_string() +
                        "." +
                        ossia::get_unit_text(settings.unit).to_string()));

        m_description->setText(settings.description);
    }
    m_tagsEdit->addItems(settings.tags);
}
}
