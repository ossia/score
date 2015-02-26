#include "MinuitProtocolSettingsWidget.hpp"

#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>

MinuitProtocolSettingsWidget::MinuitProtocolSettingsWidget (QWidget* parent)
    : ProtocolSettingsWidget (parent)
{
    buildGUI();
}

void
MinuitProtocolSettingsWidget::buildGUI()
{
    QLabel* deviceNameLabel = new QLabel (tr ("Device name"), this);
    m_deviceNameEdit = new QLineEdit (this);

    QLabel* portOutputLabel = new QLabel (tr ("Port (destination)"), this);
    m_portOutputSBox = new QSpinBox (this);
    m_portOutputSBox->setRange (0, 65535);

    QLabel* localHostLabel = new QLabel (tr ("Host"), this);
    m_localHostEdit = new QLineEdit (this);


    QGridLayout* gLayout = new QGridLayout;

    gLayout->addWidget (deviceNameLabel, 0, 0, 1, 1);
    gLayout->addWidget (m_deviceNameEdit, 0, 1, 1, 1);

    gLayout->addWidget (portOutputLabel, 1, 0, 1, 1);
    gLayout->addWidget (m_portOutputSBox, 1, 1, 1, 1);

    gLayout->addWidget (localHostLabel, 2, 0, 1, 1);
    gLayout->addWidget (m_localHostEdit, 2, 1, 1, 1);

    setLayout (gLayout);

    setDefaults();
}

void
MinuitProtocolSettingsWidget::setDefaults()
{
    Q_ASSERT (m_deviceNameEdit);
    Q_ASSERT (m_portOutputSBox);
    Q_ASSERT (m_localHostEdit);

    //TODO: we should use QSettings ?

    m_deviceNameEdit->setText ("MinuitDevice");
    m_portOutputSBox->setValue (9998);
    //m_portInputSBox->setValue(13579);
    m_localHostEdit->setText ("127.0.0.1");
}

QList<QString>
MinuitProtocolSettingsWidget::getSettings() const
{
    Q_ASSERT (m_deviceNameEdit);

    QList<QString> list;
    list.append (m_deviceNameEdit->text() ); //name first !
    list.append (QString::number (m_portOutputSBox->value() ) );
    list.append (m_localHostEdit->text() );
    return list;
}

void
MinuitProtocolSettingsWidget::setSettings (const QList<QString>& settings)
{
    Q_ASSERT (m_deviceNameEdit);
    Q_ASSERT (settings.size() == 3);
    m_deviceNameEdit->setText (settings.at (0) );
    m_portOutputSBox->setValue (settings.at (1).toInt() );
    m_localHostEdit->setText (settings.at (2) );
}
