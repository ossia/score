#include "AddressSettingsWidget.hpp"
#include <QFormLayout>
#include <QSpinBox>
#include <QLineEdit>
#include <Common/CommonTypes.hpp>

AddressSettingsWidget::AddressSettingsWidget(QWidget *parent) :
    QWidget(parent),
    m_layout{new QFormLayout}
{
    m_ioTypeCBox = new QComboBox(this);
    m_clipModeCBox = new QComboBox(this);
    m_tagsEdit = new QLineEdit(this);

    m_layout->addRow(tr("I/O type"), m_ioTypeCBox);
    m_layout->addRow(tr("Clip mode"), m_clipModeCBox);
    m_layout->addRow(tr("Tags"), m_tagsEdit);

    populateIOTypes(m_ioTypeCBox);
    populateClipMode(m_clipModeCBox);

    m_ioTypeCBox->setCurrentIndex(0);
    m_clipModeCBox->setCurrentIndex(0);

    m_tagsEdit->setText("");

    setLayout(m_layout);
}

AddressSettings AddressSettingsWidget::getCommonSettings() const
{
    AddressSettings settings;
    settings.ioType = IOTypeStringMap().key(m_ioTypeCBox->currentText());
    settings.tags = QStringList{m_tagsEdit->text()}; // TODO
    settings.clipMode = ClipModeStringMap().key(m_clipModeCBox->currentText()); // TODO use data()

    return settings;

}

void AddressSettingsWidget::setCommonSettings(const AddressSettings & settings)
{
    const int ioTypeIndex = m_ioTypeCBox->findText(IOTypeStringMap()[settings.ioType]);
    Q_ASSERT(ioTypeIndex != -1);
    m_ioTypeCBox->setCurrentIndex(ioTypeIndex);

    int clipModeIndex = m_clipModeCBox->findText(ClipModeStringMap()[settings.clipMode]);
    Q_ASSERT(clipModeIndex != -1);
    m_clipModeCBox->setCurrentIndex(clipModeIndex);

    m_tagsEdit->setText(settings.tags.join(", "));
}
