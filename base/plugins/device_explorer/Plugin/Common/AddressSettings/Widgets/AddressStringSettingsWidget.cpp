#include "AddressStringSettingsWidget.hpp"

#include <QComboBox>
#include <QDebug>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>

#include <Common/CommonTypes.hpp>


AddressStringSettingsWidget::AddressStringSettingsWidget(QWidget* parent)
    : AddressSettingsWidget(parent)
{

    buildGUI();
}

void
AddressStringSettingsWidget::buildGUI()
{
    QLabel* ioTypeLabel = new QLabel(tr("I/O type"), this);
    m_ioTypeCBox = new QComboBox(this);

    QLabel* valueLabel = new QLabel(tr("Value"), this);
    m_valueEdit = new QLineEdit(this);

    QLabel* priorityLabel  = new QLabel(tr("Priority"), this);
    m_prioritySBox = new QSpinBox(this);

    QLabel* tagsLabel  = new QLabel(tr("Tags"), this);
    m_tagsEdit = new QLineEdit(this);

    QGridLayout* gLayout = new QGridLayout;

    gLayout->addWidget(ioTypeLabel, 0, 0, 1, 1);
    gLayout->addWidget(m_ioTypeCBox, 0, 1, 1, 1);

    gLayout->addWidget(valueLabel, 1, 0, 1, 1);
    gLayout->addWidget(m_valueEdit, 1, 1, 1, 1);

    gLayout->addWidget(priorityLabel, 2, 0, 1, 1);
    gLayout->addWidget(m_prioritySBox, 2, 1, 1, 1);

    gLayout->addWidget(tagsLabel, 3, 0, 1, 1);
    gLayout->addWidget(m_tagsEdit, 3, 1, 1, 1);

    populateIOTypes(m_ioTypeCBox);

    setLayout(gLayout);

    setDefaults();
}

void
AddressStringSettingsWidget::setDefaults()
{
    Q_ASSERT(m_ioTypeCBox);
    Q_ASSERT(m_valueEdit);

    m_ioTypeCBox->setCurrentIndex(0);

    m_valueEdit->setText("");

    m_prioritySBox->setMinimum(0);
    m_prioritySBox->setMaximum(10000);  //?
    m_prioritySBox->setSingleStep(1);
    m_prioritySBox->setValue(0);

    m_tagsEdit->setText("");
}

QList<QString>
AddressStringSettingsWidget::getSettings() const
{
    Q_ASSERT(m_ioTypeCBox);

    QList<QString> list;
    list.append(m_ioTypeCBox->currentText());
    list.append(m_valueEdit->text());
    list.append(QString::number(m_prioritySBox->value()));
    list.append(m_tagsEdit->text());   //TODO: TagListWidget
    return list;
}

void
AddressStringSettingsWidget::setSettings(const QList<QString>& settings)
{
    Q_ASSERT(m_ioTypeCBox);

    Q_ASSERT(settings.size() == 4);

    const QString& ioTypeString = settings.at(0);
    const int ioTypeIndex = m_ioTypeCBox->findText(ioTypeString);

    if(ioTypeIndex != -1)
    {
        m_ioTypeCBox->setCurrentIndex(ioTypeIndex);
    }
    else
    {
        qDebug() << tr("Unknown I/O type: %1").arg(ioTypeString) << "\n";
    }

    m_valueEdit->setText(settings.at(1));

    m_prioritySBox->setValue(settings.at(2).toInt());

    m_tagsEdit->setText(settings.at(3));
}


