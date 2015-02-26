#include "OSCProtocolSettingsWidget.hpp"

#include <QFileDialog>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>


OSCProtocolSettingsWidget::OSCProtocolSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
    buildGUI();
}

void
OSCProtocolSettingsWidget::buildGUI()
{
    QLabel* deviceNameLabel = new QLabel(tr("Device name"), this);
    m_deviceNameEdit = new QLineEdit(this);

    QLabel* portOutputLabel = new QLabel(tr("Port (destination)"), this);
    m_portOutputSBox = new QSpinBox(this);
    m_portOutputSBox->setRange(0, 65535);

    QLabel* portInputLabel = new QLabel(tr("Port (reception)"), this);
    m_portInputSBox = new QSpinBox(this);
    m_portInputSBox->setRange(0, 65535);

    QLabel* localHostLabel = new QLabel(tr("Host"), this);
    m_localHostEdit = new QLineEdit(this);

    QPushButton* loadNamespaceButton = new QPushButton(tr("Load..."), this);
    loadNamespaceButton->setAutoDefault(false);
    loadNamespaceButton->setToolTip(tr("Load namespace file"));
    m_namespaceFilePathEdit = new QLineEdit(this);


    QGridLayout* gLayout = new QGridLayout;

    gLayout->addWidget(deviceNameLabel, 0, 0, 1, 1);
    gLayout->addWidget(m_deviceNameEdit, 0, 1, 1, 1);

    gLayout->addWidget(portOutputLabel, 1, 0, 1, 1);
    gLayout->addWidget(m_portOutputSBox, 1, 1, 1, 1);
    gLayout->addWidget(portInputLabel, 2, 0, 1, 1);
    gLayout->addWidget(m_portInputSBox, 2, 1, 1, 1);

    gLayout->addWidget(localHostLabel, 3, 0, 1, 1);
    gLayout->addWidget(m_localHostEdit, 3, 1, 1, 1);

    gLayout->addWidget(loadNamespaceButton, 4, 0, 1, 1);
    gLayout->addWidget(m_namespaceFilePathEdit, 4, 1, 1, 1);

    connect(loadNamespaceButton, SIGNAL(clicked()), this, SLOT(openFileDialog()));

    setLayout(gLayout);

    setDefaults();
}

void
OSCProtocolSettingsWidget::setDefaults()
{
    //TODO: we should use QSettings ?

    m_deviceNameEdit->setText("OSCdevice");
    m_portOutputSBox->setValue(9997);
    m_portInputSBox->setValue(9996);
    m_localHostEdit->setText("127.0.0.1");
}


QList<QString>
OSCProtocolSettingsWidget::getSettings() const
{
    QList<QString> list;
    list.append(m_deviceNameEdit->text());   //name first !
    list.append(QString::number(m_portOutputSBox->value()));
    list.append(QString::number(m_portInputSBox->value()));
    list.append(m_localHostEdit->text());
    list.append(m_namespaceFilePathEdit->text());
    return list;
}

void
OSCProtocolSettingsWidget::setSettings(const QList<QString>& settings)
{
    Q_ASSERT(settings.size() == 5);
    m_deviceNameEdit->setText(settings.at(0));
    m_portOutputSBox->setValue(settings.at(1).toInt());
    m_portInputSBox->setValue(settings.at(2).toInt());
    m_localHostEdit->setText(settings.at(3));
    m_namespaceFilePathEdit->setText(settings.at(4));
}



void
OSCProtocolSettingsWidget::openFileDialog()
{
    const QString fileName = QFileDialog::getOpenFileName();

    if(! fileName.isEmpty())
    {
        m_namespaceFilePathEdit->setText(fileName);
    }

}
