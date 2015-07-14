#include "IpDialog.hpp"
#include "IpWidget.hpp"
#include <QDialog>
#include <QDialogButtonBox>
#include <QSpinBox>

IpDialog::IpDialog(QWidget *parent)
{
    setLayout(new QVBoxLayout);
    auto widg = new QWidget;
    layout()->addWidget(widg);
    widg->setLayout(new QHBoxLayout);

    m_ipBox = new IpWidget{this};
    widg->layout()->addWidget(m_ipBox);

    m_portBox = new QSpinBox;
    m_portBox->setMinimum(1);
    m_portBox->setMaximum(65535);
    m_portBox->setValue(9090);
    widg->layout()->addWidget(m_portBox);

    auto box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout()->addWidget(box);

    connect(box, &QDialogButtonBox::accepted, this, &IpDialog::accepted);
    connect(box, &QDialogButtonBox::rejected, this, &IpDialog::rejected);
}

int IpDialog::port() const { return m_port; }

const QString &IpDialog::ip() const { return m_ip; }

void IpDialog::accepted()
{
    m_ip = m_ipBox->lineEdits[0]->text() + "." +
            m_ipBox->lineEdits[1]->text() + "." +
            m_ipBox->lineEdits[2]->text() + "." +
            m_ipBox->lineEdits[3]->text();

    m_port = m_portBox->value();
    accept();
}

void IpDialog::rejected()
{
    m_port = 0;
    reject();
}
