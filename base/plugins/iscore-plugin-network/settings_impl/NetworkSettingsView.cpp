#include "NetworkSettingsView.hpp"
#include <QGridLayout>
#include <QLabel>
#include "NetworkSettingsPresenter.hpp"
#include "NetworkSettingsModel.hpp"
#include "commands/ClientPortChangedCommand.hpp"
#include "commands/MasterPortChangedCommand.hpp"
#include "commands/ClientNameChangedCommand.hpp"
#include <QApplication>
using namespace iscore;

NetworkSettingsView::NetworkSettingsView(QObject* parent) :
    iscore::SettingsDelegateViewInterface {parent}
{
    auto layout = new QGridLayout(m_widget);
    m_widget->setLayout(layout);

    m_masterPort->setMinimum(1001);
    m_clientPort->setMinimum(1001);
    m_masterPort->setMaximum(65535);
    m_clientPort->setMaximum(65535);

    layout->addWidget(new QLabel{"Master port"}, 0, 0);
    layout->addWidget(m_masterPort, 0, 1);

    layout->addWidget(new QLabel{"Client port"}, 1, 0);
    layout->addWidget(m_clientPort, 1, 1);

    layout->addWidget(new QLabel{"Client Name"}, 2, 0);
    layout->addWidget(m_clientName, 2, 1);
}

void NetworkSettingsView::setClientName(QString text)
{
    if(text != m_clientName->text())
    {
        m_clientName->setText(text);
    }
}
void NetworkSettingsView::setMasterPort(int val)
{
    if(val != m_masterPort->value())
    {
        m_masterPort->setValue(val);
    }
}
void NetworkSettingsView::setClientPort(int val)
{
    if(val != m_clientPort->value())
    {
        m_clientPort->setValue(val);
    }
}

QWidget* NetworkSettingsView::getWidget()
{
    return m_widget;
}

void NetworkSettingsView::load()
{
    m_previousMasterPort = m_masterPort->value();
    m_previousClientPort = m_clientPort->value();
    m_previousClientName = m_clientName->text();
}

void NetworkSettingsView::doConnections()
{
    connect(m_clientName,	&QLineEdit::textChanged,
            this,			&NetworkSettingsView::on_clientNameChanged);

    // http://stackoverflow.com/questions/16794695/qt5-overloaded-signals-and-slots
    connect(m_masterPort,	SIGNAL(valueChanged(int)),
            this,			SLOT(on_masterPortChanged(int)));
    connect(m_clientPort,	SIGNAL(valueChanged(int)),
            this,			SLOT(on_clientPortChanged(int)));
}

void NetworkSettingsView::on_masterPortChanged(int x)
{
    /*
    auto newVal = m_masterPort->value();

    if(newVal != m_previousMasterPort)
    {
        presenter()->setMasterPortCommand(new MasterPortChangedCommand {m_previousMasterPort, newVal});
        m_previousMasterPort = newVal;
    }*/
}

void NetworkSettingsView::on_clientPortChanged(int)
{
    /*
    auto newVal = m_clientPort->value();

    if(newVal != m_previousClientPort)
    {
        presenter()->setClientPortCommand(new ClientPortChangedCommand {m_previousClientPort, newVal});
        m_previousClientPort = newVal;
    }
    */
}
void NetworkSettingsView::on_clientNameChanged()
{
    /*
    auto newText = m_clientName->text();

    if(newText != m_previousClientName)
    {
        presenter()->setClientNameCommand(new ClientNameChangedCommand {m_previousClientName, newText});
        m_previousClientName = newText;
    }*/
}

NetworkSettingsPresenter* NetworkSettingsView::presenter()
{
    return static_cast<NetworkSettingsPresenter*>(m_presenter);
}
