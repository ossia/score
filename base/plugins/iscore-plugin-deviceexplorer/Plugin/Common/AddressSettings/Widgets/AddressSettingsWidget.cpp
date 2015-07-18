#include "AddressSettingsWidget.hpp"
#include <QFormLayout>
#include <QSpinBox>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QInputDialog>

AddressSettingsWidget::AddressSettingsWidget(QWidget *parent) :
    QWidget(parent),
    m_layout{new QFormLayout}
{
    m_ioTypeCBox = new QComboBox{this};
    m_clipModeCBox = new QComboBox{this};
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

    m_layout->addRow(tr("I/O type"), m_ioTypeCBox);
    m_layout->addRow(tr("Clip mode"), m_clipModeCBox);
    m_layout->addRow(tr("Tags"), tagLayout);

    // Populate the combo boxes
    for(const auto& key : iscore::IOTypeStringMap().keys())
    {
        m_ioTypeCBox->addItem(iscore::IOTypeStringMap()[key], (int)key);
    }

    for(const auto& key : iscore::ClipModeStringMap().keys())
    {
        m_clipModeCBox->addItem(iscore::ClipModeStringMap()[key], (int)key);
    }

    setLayout(m_layout);
}

AddressSettings AddressSettingsWidget::getCommonSettings() const
{
    AddressSettings settings;
    settings.ioType = static_cast<iscore::IOType>(m_ioTypeCBox->currentData().value<int>());
    settings.clipMode = static_cast<iscore::ClipMode>(m_clipModeCBox->currentData().value<int>());

    for(int i = 0; i < m_tagsEdit->count(); i++)
        settings.tags.append(m_tagsEdit->itemText(i));

    return settings;
}

void AddressSettingsWidget::setCommonSettings(const AddressSettings & settings)
{
    const int ioTypeIndex = m_ioTypeCBox->findData((int)settings.ioType);
    Q_ASSERT(ioTypeIndex != -1);
    m_ioTypeCBox->setCurrentIndex(ioTypeIndex);

    const int clipModeIndex = m_clipModeCBox->findData((int)settings.clipMode);
    Q_ASSERT(clipModeIndex != -1);
    m_clipModeCBox->setCurrentIndex(clipModeIndex);

    m_tagsEdit->addItems(settings.tags);
}
